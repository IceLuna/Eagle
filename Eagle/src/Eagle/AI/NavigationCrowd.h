#pragma once

#include "Eagle/Core/Timestep.h"

#include <DetourCrowd.h>
#include <glm/glm.hpp>

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
		// Detour's proximity grid stores 4 entries per agent in 16-bit indices (0xffff is its null index),
		// so 4 * MaxAgents must stay below 65535
		static constexpr uint32_t MaxAgentsLimit = 16383u;
		// The proximity grid's cell size is 3 * MaxAgentRadius and must be positive
		static constexpr float MinAgentRadius = 0.01f;

		uint32_t MaxAgents = 256u;
		float MaxAgentRadius = 0.6f;

		void Clamp()
		{
			MaxAgents = glm::clamp(MaxAgents, 1u, MaxAgentsLimit);
			MaxAgentRadius = glm::max(MaxAgentRadius, MinAgentRadius);
		}
	};

	class Crowd
	{
	public:
		Crowd();
		~Crowd();

		// Returns false if failed
		bool Init(const CrowdSettings& settings, dtNavMesh* navMesh, dtNavMeshQuery* navQuery, const dtQueryFilter& filter);
		void Release();
		bool IsInitialized() const { return m_Crowd != nullptr; }

		bool SetSettings(const CrowdSettings& settings);
		const CrowdSettings& GetSettings() const { return m_Settings; }

		// Returns an index of an agent
		int AddAgent(const glm::vec3& pos, const AgentSettings& settings);
		void RemoveAgent(int agentIndex);
		int GetAgentCount() const;

		bool GetAgentLocation(int agentIndex, glm::vec3* outLocation) const;
		bool GetAgentVelocity(int agentIndex, glm::vec3* outVelocity) const;

		// Moves the agent keeping its path.
		// If the location is far from the agent, it's placed at the nearest point of the nav mesh, and its move target is requested again.
		// @velocity. The entity's actual velocity, so that the crowd steers from it. Only its horizontal part is used
		void SetAgentLocation(int agentIndex, const glm::vec3& location, const glm::vec3& velocity);
		MoveRequestState GetAgentTargetState(int agentIndex) const;

		// The target is snapped to the nearest point within `searchHalfExtents` of the nav mesh.
		// If it's not set, the nav mesh's `SearchHalfExtents` are used.
		// Returns false and keeps the current target if there's no nav mesh there
		bool SetMoveTarget(int agentIndex, const glm::vec3& target, const std::optional<glm::vec3>& searchHalfExtents = {});
		bool SetMoveTarget(const glm::vec3& target, const std::optional<glm::vec3>& searchHalfExtents = {});

		void SetSearchHalfExtents(const glm::vec3& searchHalfExtents) { m_SearchHalfExtents = searchHalfExtents; }

		void ResetMoveTarget(int agentIndex);
		void ResetMoveTarget();

		void UpdateAgentSettings(int agentIndex, const AgentSettings& settings);

		void Update(Timestep ts);

	private:
		// Returns 0 if there's no polygon within `searchHalfExtents` of `pos`
		dtPolyRef FindNearestPoly(const glm::vec3& pos, const std::optional<glm::vec3>& searchHalfExtents, glm::vec3* outTargetPos) const;

	private:
		dtCrowd* m_Crowd = nullptr;
		dtPolyRef m_TargetRef{};
		glm::vec3 m_TargetPos{};
		glm::vec3 m_SearchHalfExtents = glm::vec3(0.9f, 2.f, 0.9f);

		// Not owning
		dtNavMesh* m_NavMesh = nullptr;
		dtNavMeshQuery* m_NavQuery = nullptr;
		dtQueryFilter m_Filter{};

		CrowdSettings m_Settings{};
	};
}
