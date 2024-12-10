#pragma once

#include "Eagle/Core/Timestep.h"

#include <DetourCrowd.h>

namespace Eagle::AINavigation
{
	enum class AgentObstacleAvoidanceQuality
	{
		Low, Medium, Good, High
	};

	struct AgentSettings
	{
		float AgentRadius = 0.6f;
		float AgentHeight = 2.f;
		float MaxAcceleration = 8.f;
		float MaxSpeed = 3.5f;
		float SeparationWeight = 2.f;
		AgentObstacleAvoidanceQuality ObstacleAvoidanceQuality = AgentObstacleAvoidanceQuality::Good;
		bool bAnticipateTurns = true;
		bool bOptimizeVis = true;
		bool bOptimizeTopo = true;
		bool bSeparation = false;
	};

	struct CrowdSettings
	{
		uint32_t MaxAgents = 256u;
		float MaxAgentRadius = 0.6f;
	};

	class Crowd
	{
	public:
		Crowd();
		~Crowd();

		void Init(const CrowdSettings& settings, dtNavMesh* navMesh, dtNavMeshQuery* navQuery, const dtQueryFilter& filter);
		void Release();

		void SetSettings(const CrowdSettings& settings);
		const CrowdSettings& GetSettings() const { return m_Settings; }

		// Returns an index of an agent
		int AddAgent(const glm::vec3& pos, const AgentSettings& settings);
		void RemoveAgent(int agentIndex);
		int GetAgentCount() const;

		bool GetAgentLocation(int agentIndex, glm::vec3* outLocation) const;
		bool GetAgentVelocity(int agentIndex, glm::vec3* outVelocity) const;
		MoveRequestState GetAgentTargetState(int agentIndex) const;

		void SetMoveTarget(int agentIndex, const glm::vec3& pos);
		void SetMoveTarget(const glm::vec3& pos);

		void ResetMoveTarget(int agentIndex);
		void ResetMoveTarget();

		void UpdateAgentSettings(int agentIndex, const AgentSettings& settings);

		void Update(Timestep ts);

	private:
		dtCrowd* m_Crowd = nullptr;
		dtPolyRef m_TargetRef{};
		glm::vec3 m_TargetPos{};

		// Not owning
		dtNavMesh* m_NavMesh = nullptr;
		dtNavMeshQuery* m_NavQuery = nullptr;
		dtQueryFilter m_Filter{};

		CrowdSettings m_Settings{};
	};
}
