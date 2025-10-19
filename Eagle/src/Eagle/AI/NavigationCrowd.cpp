#include "egpch.h"
#include "NavigationCrowd.h"
#include "NavigationUtils.h"

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

	Crowd::Crowd()
	{}
	
	Crowd::~Crowd()
	{
		Release();
	}
	
	void Crowd::Init(const CrowdSettings& settings, dtNavMesh* navMesh, dtNavMeshQuery* navQuery, const dtQueryFilter& filter)
	{
		m_Settings = settings;
		m_NavMesh = navMesh;
		m_NavQuery = navQuery;
		m_Filter = filter;

		EG_CORE_ASSERT(!m_Crowd);

		m_Crowd = dtAllocCrowd();
		if (!m_Crowd)
		{
			EG_CORE_ERROR("[AINavigation] Failed to allocate crowd");
			return;
		}

		m_Crowd->init(m_Settings.MaxAgents, m_Settings.MaxAgentRadius, navMesh);

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
	}

	void Crowd::Release()
	{
		dtFreeCrowd(m_Crowd);
		m_Crowd = nullptr;
	}

	void Crowd::SetSettings(const CrowdSettings& settings)
	{
		Release();
		Init(settings, m_NavMesh, m_NavQuery, m_Filter);
	}

	int Crowd::AddAgent(const glm::vec3& pos, const AgentSettings& settings)
	{
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
		m_Crowd->removeAgent(agentIndex);
	}

	int Crowd::GetAgentCount() const
	{
		return m_Crowd->getAgentCount();
	}

	bool Crowd::GetAgentLocation(int agentIndex, glm::vec3* outLocation) const
	{
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
		const dtCrowdAgent* ag = m_Crowd->getAgent(agentIndex);
		if (ag && ag->active)
		{
			return MoveRequestState(ag->targetState);
		}

		return MoveRequestState::DT_CROWDAGENT_TARGET_NONE;
	}
	
	void Crowd::SetMoveTarget(int agentIndex, const glm::vec3& pos)
	{
		const float searchHalfExtent[3] = { 2, 4, 2 };
		dtPolyRef targetRef;
		glm::vec3 targetPos;
		m_NavQuery->findNearestPoly(&pos.x, searchHalfExtent, &m_Filter, &targetRef, &targetPos.x);

		const dtCrowdAgent* ag = m_Crowd->getAgent(agentIndex);
		if (ag && ag->active)
			m_Crowd->requestMoveTarget(agentIndex, targetRef, &targetPos.x);
	}
	
	void Crowd::SetMoveTarget(const glm::vec3& pos)
	{
		const float searchHalfExtent[3] = { 2, 4, 2 };
		m_NavQuery->findNearestPoly(&pos.x, searchHalfExtent, &m_Filter, &m_TargetRef, &m_TargetPos.x);

		for (int i = 0; i < m_Crowd->getAgentCount(); ++i)
		{
			const dtCrowdAgent* ag = m_Crowd->getAgent(i);
			if (!ag->active)
				continue;

			m_Crowd->requestMoveTarget(i, m_TargetRef, &m_TargetPos.x);
		}
	}

	void Crowd::ResetMoveTarget(int agentIndex)
	{
		const dtCrowdAgent* ag = m_Crowd->getAgent(agentIndex);
		if (!ag || !ag->active)
			return;

		m_Crowd->resetMoveTarget(agentIndex);
	}

	void Crowd::ResetMoveTarget()
	{
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
		const dtCrowdAgent* ag = m_Crowd->getAgent(agentIndex);
		if (!ag || !ag->active)
			return;

		dtCrowdAgentParams params = AgentSettingsToDetour(settings);
		m_Crowd->updateAgentParameters(agentIndex, &params);
	}
	
	void Crowd::Update(Timestep ts)
	{
		m_Crowd->update(ts, nullptr);
	}
}
