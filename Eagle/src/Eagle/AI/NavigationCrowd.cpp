#include "egpch.h"
#include "NavigationCrowd.h"
#include "NavigationUtils.h"

#include "Eagle/Core/Application.h"

namespace Eagle::AINavigation
{
	static dtCrowdAgentParams AgentSettingsToDetour(const AgentSettings& settings)
	{
		constexpr bool bObstacleAvoidance = true;

		dtCrowdAgentParams ap;
		memset(&ap, 0, sizeof(ap));
		ap.radius = settings.AgentRadius;
		ap.height = settings.AgentHeight;
		ap.maxAcceleration = settings.MaxAcceleration;
		ap.maxSpeed = settings.MaxSpeed;
		ap.collisionQueryRange = ap.radius * 12.0f;
		ap.pathOptimizationRange = ap.radius * 30.0f;
		ap.updateFlags = 0;
		if (settings.bAnticipateTurns)
			ap.updateFlags |= DT_CROWD_ANTICIPATE_TURNS;
		if (settings.bOptimizeVis)
			ap.updateFlags |= DT_CROWD_OPTIMIZE_VIS;
		if (settings.bOptimizeTopo)
			ap.updateFlags |= DT_CROWD_OPTIMIZE_TOPO;
		if (bObstacleAvoidance)
			ap.updateFlags |= DT_CROWD_OBSTACLE_AVOIDANCE;
		if (settings.bSeparation)
			ap.updateFlags |= DT_CROWD_SEPARATION;
		ap.obstacleAvoidanceType = (unsigned char)settings.ObstacleAvoidanceQuality;
		ap.separationWeight = settings.SeparationWeight;

		return ap;
	}

	static void ReportError(std::string&& message)
	{
		EG_CORE_ERROR(message);
		Application::Get().GetImGuiLayer()->AddMessage(std::move(message));
	}

	Crowd::Crowd()
	{}
	
	Crowd::~Crowd()
	{
		Release();
	}
	
	bool Crowd::Init(const CrowdSettings& settings, dtNavMesh* navMesh, dtNavMeshQuery* navQuery, const dtQueryFilter& filter)
	{
		m_Settings = settings;
		m_Settings.Clamp();
		m_NavMesh = navMesh;
		m_NavQuery = navQuery;
		m_Filter = filter;
		m_TargetRef = 0u;

		EG_CORE_ASSERT(!m_Crowd);

		if (!m_NavMesh || !m_NavQuery)
		{
			EG_CORE_ERROR("[AINavigation] Failed to set up the crowd. There's no navigation mesh");
			return false;
		}

		m_Crowd = dtAllocCrowd();
		if (!m_Crowd)
		{
			EG_CORE_ERROR("[AINavigation] Failed to allocate crowd");
			return false;
		}

		if (!m_Crowd->init((int)m_Settings.MaxAgents, m_Settings.MaxAgentRadius, navMesh))
		{
			ReportError(std::format("[AINavigation] Failed to initialize the crowd (Max Agents: {}, Max Agent Radius: {}). Crowd agents won't work", m_Settings.MaxAgents, m_Settings.MaxAgentRadius));
			Release();
			return false;
		}

		// Make polygons with 'disabled' flag invalid.
		m_Crowd->getEditableFilter(0)->setExcludeFlags(SAMPLE_POLYFLAGS_DISABLED);

		// Setup local avoidance params to different qualities.
		dtObstacleAvoidanceParams params;
		// Use mostly default settings, copy from dtCrowd.
		memcpy(&params, m_Crowd->getObstacleAvoidanceParams(0), sizeof(dtObstacleAvoidanceParams));

		// Low (11)
		params.velBias = 0.5f;
		params.adaptiveDivs = 5;
		params.adaptiveRings = 2;
		params.adaptiveDepth = 1;
		m_Crowd->setObstacleAvoidanceParams(0, &params);
		static_assert(int(AgentObstacleAvoidanceQuality::Low) == 0);

		// Medium (22)
		params.velBias = 0.5f;
		params.adaptiveDivs = 5;
		params.adaptiveRings = 2;
		params.adaptiveDepth = 2;
		m_Crowd->setObstacleAvoidanceParams(1, &params);
		static_assert(int(AgentObstacleAvoidanceQuality::Medium) == 1);

		// Good (45)
		params.velBias = 0.5f;
		params.adaptiveDivs = 7;
		params.adaptiveRings = 2;
		params.adaptiveDepth = 3;
		m_Crowd->setObstacleAvoidanceParams(2, &params);
		static_assert(int(AgentObstacleAvoidanceQuality::Good) == 2);

		// High (66)
		params.velBias = 0.5f;
		params.adaptiveDivs = 7;
		params.adaptiveRings = 3;
		params.adaptiveDepth = 3;

		m_Crowd->setObstacleAvoidanceParams(3, &params);
		static_assert(int(AgentObstacleAvoidanceQuality::High) == 3);

		return true;
	}

	void Crowd::Release()
	{
		dtFreeCrowd(m_Crowd);
		m_Crowd = nullptr;
	}

	bool Crowd::SetSettings(const CrowdSettings& settings)
	{
		Release();
		return Init(settings, m_NavMesh, m_NavQuery, m_Filter);
	}

	int Crowd::AddAgent(const glm::vec3& pos, const AgentSettings& settings)
	{
		if (!m_Crowd)
		{
			EG_CORE_ERROR("Failed to add a crowd agent. Crowd System isn't initialized");
			return -1;
		}

		dtCrowdAgentParams ap = AgentSettingsToDetour(settings);
		int idx = m_Crowd->addAgent(&pos.x, &ap);
		if (idx != -1)
		{
			if (m_TargetRef)
				m_Crowd->requestMoveTarget(idx, m_TargetRef, &m_TargetPos.x);
		}

		return idx;
	}
	
	void Crowd::RemoveAgent(int agentIndex)
	{
		if (!m_Crowd)
		{
			EG_CORE_ERROR("Failed to remove a crowd agent. Crowd System isn't initialized");
			return;
		}

		m_Crowd->removeAgent(agentIndex);
	}

	int Crowd::GetAgentCount() const
	{
		if (!m_Crowd)
		{
			EG_CORE_ERROR("Failed to get crowd agent count. Crowd System isn't initialized");
			return 0;
		}

		return m_Crowd->getAgentCount();
	}

	bool Crowd::GetAgentLocation(int agentIndex, glm::vec3* outLocation) const
	{
		if (!m_Crowd)
		{
			EG_CORE_ERROR("Failed to get crowd agent location. Crowd System isn't initialized");
			return false;
		}

		const dtCrowdAgent* ag = m_Crowd->getAgent(agentIndex);
		if (ag && ag->active)
		{
			*outLocation = glm::vec3(ag->npos[0], ag->npos[1], ag->npos[2]);
			return true;
		}
		
		return false;
	}

	bool Crowd::GetAgentVelocity(int agentIndex, glm::vec3* outVelocity) const
	{
		if (!m_Crowd)
		{
			EG_CORE_ERROR("Failed to get crowd agent velocity. Crowd System isn't initialized");
			return false;
		}

		const dtCrowdAgent* ag = m_Crowd->getAgent(agentIndex);
		if (ag && ag->active)
		{
			*outVelocity = glm::vec3(ag->vel[0], ag->vel[1], ag->vel[2]);
			return true;
		}
		
		return false;
	}

	MoveRequestState Crowd::GetAgentTargetState(int agentIndex) const
	{
		if (!m_Crowd)
		{
			EG_CORE_ERROR("Failed to get crowd agent target state. Crowd System isn't initialized");
			return MoveRequestState::DT_CROWDAGENT_TARGET_NONE;
		}

		const dtCrowdAgent* ag = m_Crowd->getAgent(agentIndex);
		if (ag && ag->active)
		{
			return MoveRequestState(ag->targetState);
		}

		return MoveRequestState::DT_CROWDAGENT_TARGET_NONE;
	}
	
	dtPolyRef Crowd::FindNearestPoly(const glm::vec3& pos, const std::optional<glm::vec3>& searchHalfExtents, glm::vec3* outTargetPos) const
	{
		const glm::vec3& extents = searchHalfExtents ? *searchHalfExtents : m_SearchHalfExtents;
		if (extents.x <= 0.f || extents.y <= 0.f || extents.z <= 0.f)
			return 0u;

		dtPolyRef poly = 0u;
		if (dtStatusFailed(m_NavQuery->findNearestPoly(&pos[0], &extents[0], &m_Filter, &poly, &outTargetPos->x)))
			return 0;

		return poly;
	}

	bool Crowd::SetMoveTarget(int agentIndex, const glm::vec3& target, const std::optional<glm::vec3>& searchHalfExtents)
	{
		if (!m_Crowd)
		{
			EG_CORE_ERROR("Failed to set crowd agent move target. Crowd System isn't initialized");
			return false;
		}

		const dtCrowdAgent* ag = m_Crowd->getAgent(agentIndex);
		if (!ag || !ag->active)
			return false;

		glm::vec3 targetPos = target;
		const dtPolyRef targetRef = FindNearestPoly(target, searchHalfExtents, &targetPos);
		if (targetRef == 0u)
		{
			EG_CORE_ERROR("Failed to set the crowd move target for the agent at index {}. No nav mesh polygon near the target (or an invalid position)", agentIndex);
			return false;
		}

		return m_Crowd->requestMoveTarget(agentIndex, targetRef, &targetPos.x);
	}
	
	bool Crowd::SetMoveTarget(const glm::vec3& target, const std::optional<glm::vec3>& searchHalfExtents)
	{
		if (!m_Crowd)
		{
			EG_CORE_ERROR("Failed to set crowd agents move target. Crowd System isn't initialized");
			return false;
		}

		glm::vec3 targetPos = target;
		const dtPolyRef targetRef = FindNearestPoly(target, searchHalfExtents, &targetPos);
		if (targetRef == 0u)
		{
			// The current target is kept
			EG_CORE_ERROR("Failed to set the crowd move target. No nav mesh polygon near the target (or an invalid position)");
			return false;
		}

		m_TargetRef = targetRef;
		m_TargetPos = targetPos;

		for (int i = 0; i < m_Crowd->getAgentCount(); ++i)
		{
			const dtCrowdAgent* ag = m_Crowd->getAgent(i);
			if (!ag->active)
				continue;

			m_Crowd->requestMoveTarget(i, m_TargetRef, &m_TargetPos.x);
		}

		return true;
	}

	void Crowd::ResetMoveTarget(int agentIndex)
	{
		if (!m_Crowd)
		{
			EG_CORE_ERROR("Failed to reset crowd agent move target. Crowd System isn't initialized");
			return;
		}

		const dtCrowdAgent* ag = m_Crowd->getAgent(agentIndex);
		if (!ag || !ag->active)
			return;

		m_Crowd->resetMoveTarget(agentIndex);
	}

	void Crowd::ResetMoveTarget()
	{
		if (!m_Crowd)
		{
			EG_CORE_ERROR("Failed to reset crowd agents move target. Crowd System isn't initialized");
			return;
		}

		m_TargetRef = 0u;

		for (int i = 0; i < m_Crowd->getAgentCount(); ++i)
		{
			const dtCrowdAgent* ag = m_Crowd->getAgent(i);
			if (!ag->active)
				continue;

			m_Crowd->resetMoveTarget(i);
		}
	}
	
	void Crowd::UpdateAgentSettings(int agentIndex, const AgentSettings& settings)
	{
		if (!m_Crowd)
		{
			EG_CORE_ERROR("Failed to update crowd agent settings. Crowd System isn't initialized");
			return;
		}

		const dtCrowdAgent* ag = m_Crowd->getAgent(agentIndex);
		if (!ag || !ag->active)
			return;

		dtCrowdAgentParams params = AgentSettingsToDetour(settings);
		m_Crowd->updateAgentParameters(agentIndex, &params);
	}
	
	void Crowd::Update(Timestep ts)
	{
		if (!m_Crowd)
			return;

		m_Crowd->update(ts, nullptr);
	}
}
