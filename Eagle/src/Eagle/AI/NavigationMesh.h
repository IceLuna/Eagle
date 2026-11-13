#pragma once

#include "Eagle/AI/NavigationUtils.h"
#include "Eagle/AI/NavigationCrowd.h"
#include "Eagle/Math/AABB.h"
#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Core/Timestep.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourTileCache.h>
#include <DebugDraw.h>

namespace Eagle
{
	struct OverlapGeometryData;
}

namespace Eagle::AINavigation
{
	inline constexpr uint32_t MaxAgentTypes = 8u;
	inline constexpr uint32_t InvalidAgentTypeID = 0xFFFFFFFFu;

	// Where an agent type can be picked automatically (crowd agents, `NavAgentType` script fields), this ID means Auto. No agent type has it
	inline constexpr uint32_t AutoAgentTypeID = 0u;

	// The agent type a nav mesh is built for
	struct AgentType
	{
		uint32_t ID = InvalidAgentTypeID;
		std::string Name;
		glm::vec3 DebugColor = glm::vec3(0.f, 0.6f, 1.f);
		float Radius = 0.6f;
		float Height = 2.f;
		float MaxClimb = 0.25f;
		float MaxSlope = 45.f;

		void Clamp()
		{
			Radius   = glm::max(Radius, 0.f);
			Height   = glm::max(Height, 0.001f);
			MaxClimb = glm::max(MaxClimb, 0.f);
			MaxSlope = glm::clamp(MaxSlope, 0.f, 90.f);
		}

		bool HasSameDimensions(const AgentType& other) const
		{
			return Radius == other.Radius && Height == other.Height && MaxClimb == other.MaxClimb && MaxSlope == other.MaxSlope;
		}
	};

	struct MeshSettings
	{
		// Limits of Recast/Detour
		static constexpr uint32_t MaxTileSizeLimit = 255u;     // Tile layer width/height are stored in 8 bits
		static constexpr uint32_t MinTileSizeLimit = 8u;       // Detour's tile cache builds broken polygons for small tiles
		static constexpr uint32_t MinBorderSizeLimit = 2u;     // Below 2, tiles aren't linked to their neighbors
		static constexpr uint32_t MaxLayersLimit = 63u;        // RC_MAX_LAYERS
		static constexpr uint32_t MaxObstaclesLimit = 65535u;  // Obstacle refs store the index in 16 bits
		static constexpr uint32_t MaxQueryNodesLimit = 65535u; // dtNavMeshQuery::init
		static constexpr uint32_t MaxTileBits = 14u;           // At most 2^14 = 16384 tile layers can be addressed
		static constexpr float MinSearchExtent = 0.01f;

		AABB Bounds = { glm::vec3(-0.5f), glm::vec3(0.5f) };
		uint32_t MaxQueryNodes = 2048u;
		uint32_t ExpectedLayersPerTile = 4u;
		uint32_t MaxLayers = 32u;
		uint32_t MaxObstacles = 128u;
		uint32_t TileSize = 8u;
		float CellSize = 0.2f;
		float CellHeight = 0.1f;
		float EdgeMaxError = 1.3f;
		uint32_t BorderSize = 3u;
		
		// Extents around a point that's searched for the nav mesh: the start/end of a path, a crowd agent's move target, etc.
		// Points farther from the nav mesh are treated as being off it. Only used if `OverrideSearchExtents` is set. Queries can override it.
		glm::vec3 SearchHalfExtents = glm::vec3(0.9f, 2.f, 0.9f);
		bool OverrideSearchExtents = false;

		bool FilterLowHangingObstacles = true;
		bool FilterLedgeSpans = true;
		bool FilterWalkableLowHeightSpans = true;

		bool operator==(const MeshSettings&) const = default;

		void Clamp()
		{
			const glm::vec3 boundsMin = glm::min(Bounds.Min, Bounds.Max);
			const glm::vec3 boundsMax = glm::max(Bounds.Min, Bounds.Max);
			Bounds.Min = boundsMin;
			Bounds.Max = boundsMax;

			MaxQueryNodes         = glm::clamp(MaxQueryNodes, 1u, MaxQueryNodesLimit);
			MaxLayers             = glm::clamp(MaxLayers, 1u, MaxLayersLimit);
			ExpectedLayersPerTile = glm::clamp(ExpectedLayersPerTile, 1u, MaxLayersLimit);
			MaxObstacles          = glm::clamp(MaxObstacles, 1u, MaxObstaclesLimit);
			TileSize              = glm::clamp(TileSize, MinTileSizeLimit, MaxTileSizeLimit);
			BorderSize            = glm::max(BorderSize, MinBorderSizeLimit);

			CellSize      = glm::max(CellSize, 0.005f);
			CellHeight    = glm::max(CellHeight, 0.001f);
			EdgeMaxError  = glm::max(EdgeMaxError, 0.f);
			SearchHalfExtents = glm::max(SearchHalfExtents, glm::vec3(MinSearchExtent));
		}

		// How far horizontally from the nav mesh an agent standing on it can reach
		float GetAgentReach(const AgentType& agentType) const
		{
			const float cellSize = glm::max(CellSize, 0.005f);
			return std::ceil(agentType.Radius / cellSize) * cellSize + cellSize;
		}

		// Covers points just off the nav mesh, and points up to Agent Height above or below it
		glm::vec3 GetAutoSearchHalfExtents(const AgentType& agentType) const
		{
			const float reach = GetAgentReach(agentType);
			return glm::max(glm::vec3(reach, agentType.Height, reach), glm::vec3(MinSearchExtent));
		}

		glm::vec3 GetSearchHalfExtents(const AgentType& agentType) const { return OverrideSearchExtents ? SearchHalfExtents : GetAutoSearchHalfExtents(agentType); }
	};

	class Mesh
	{
	public:
		Mesh(const OverlapGeometryData& tiles, const MeshSettings& meshSettings, const AgentType& agentType, const CrowdSettings& crowdSettings);
		~Mesh();

		// Updates the tile cache by rebuilding tiles touched by unfinished obstacle requests (up to `MaxTileRebuildsPerUpdate`).
		// Also updates the crowd
		void Update(Timestep ts);

		dtObstacleRef AddCylinderObstacle(const glm::vec3& pos, float radius, float height);
		// Can be rotated around Y
		dtObstacleRef AddBoxObstacle(const glm::vec3& pos, const glm::vec3& halfExtents, float yRadians);
		bool RemoveObstacle(dtObstacleRef obstacle); // Returns true if success

		// Queries that take points snap them to the nearest point within `searchHalfExtents` of the nav mesh. If it's not set, `GetSearchHalfExtents()` is used.
		std::vector<glm::vec3> FindStraightPath(const glm::vec3& start, const glm::vec3& end, uint32_t maxPolys = 256u, const std::optional<glm::vec3>& searchHalfExtents = {}) const;
		std::vector<glm::vec3> FindSmoothPath(const glm::vec3& start, const glm::vec3& end, uint32_t maxPolys = 256u, uint32_t maxSmooth = 2048u, const std::optional<glm::vec3>& searchHalfExtents = {}) const;
		bool FindDistanceToWall(const glm::vec3& pos, float maxRadius, glm::vec3* outHitPos, glm::vec3* outHitNormal, float* outHitDistance, const std::optional<glm::vec3>& searchHalfExtents = {}) const;
		bool FindRandomPoint(glm::vec3* outRandomPoint) const;
		bool FindRandomPointInCircle(const glm::vec3& pos, float radius, glm::vec3* outRandomPoint, const std::optional<glm::vec3>& searchHalfExtents = {}) const;

		// Returns true if an agent standing on the nav mesh can occupy/reach `pos`, meaning there's a nav mesh point under it, or right next to it within the agent's radius horizontally.
		// And `pos` is at most the agent's `Height` above it and at most its `MaxClimb` below it.
		// So, for example, it's true for a point at an agent's chest level, but not for a point high above the floor or under it.
		// Optionally returns that nav mesh point
		bool IsValidPoint(const glm::vec3& pos, glm::vec3* outNavMeshPoint = nullptr) const;

		// Finds the nearest point on the nav mesh within `searchHalfExtents` of `pos`. Returns false if failed
		bool FindNearestPoint(const glm::vec3& pos, glm::vec3* outNearestPoint, const std::optional<glm::vec3>& searchHalfExtents = {}) const;

		// Returns true if there's a path from `from` to `to`.
		// Very long paths might be reported as unreachable if the search runs out of `MaxQueryNodes`
		bool HasCompletePath(const glm::vec3& from, const glm::vec3& to, const std::optional<glm::vec3>& searchHalfExtents = {}) const;

		glm::vec3 GetSearchHalfExtents() const { return m_Settings.GetSearchHalfExtents(m_AgentType); }

		void SetSearchHalfExtents(const std::optional<glm::vec3>& searchHalfExtents);

		void GetDebugDraw(duDebugDraw* debugDraw) const;
		const rcConfig& GetConfig() const { return m_Config; }
		const AABB& GetAABB() const { return m_Settings.Bounds; }
		const MeshSettings& GetSettings() const { return m_Settings; }
		const AgentType& GetAgentType() const { return m_AgentType; }

		Crowd& GetCrowd() { return m_Crowd; }
		const Crowd& GetCrowd() const { return m_Crowd; }

		// Returns null if the build failed
		static Ref<Mesh> Create(const OverlapGeometryData& tiles, const MeshSettings& settings, const AgentType& agentType, const CrowdSettings& crowdSettings);

		// Per-frame budget for `Update()`.
		// Detour's tile cache rebuilds one tile per `dtTileCache::update` call.
		// Leftover work continues on the next frame
		static constexpr uint32_t MaxTileRebuildsPerUpdate = 64u;

	private:

		bool InitConfig();
		bool UpdateTileCache(float dt, uint32_t maxCalls); // Returns true if it's up to date
		void FlushObstacleRequests(); // Processes queued obstacle requests right away, and rebuilds all their tiles
		void ReserveBatchTiles(const dtCompressedTileRef* tiles, uint32_t count);
		void ReserveBatchTiles(const float* bmin, const float* bmax);
		void WarnIfObstacleTouchesTooManyTiles(const float* bmin, const float* bmax);
		uint32_t GetMaxTilesPerBatch() const { return (uint32_t)m_TileCache->getMaxTileUpdates(); }
		bool CreateTileCache();
		bool CreateNavigationMesh();
		bool RasterizeInputPolygonSoup(int x, int y);
		bool PartitionWalkableSurfaceToSimpleRegions(int x, int y);
		bool AttachNavigationTileToMesh(NavigationTileData& tileData);

		// Returns 0 if there's no polygon within `searchHalfExtents` of `pos`
		dtPolyRef FindNearestPoly(const glm::vec3& pos, const std::optional<glm::vec3>& searchHalfExtents, glm::vec3* outNearestPoint = nullptr) const;

	private:
		dtTileCacheAlloc m_Allocator;
		Scope<TileCacheCompressor> m_Compressor = nullptr;
		Scope<TileCacheMeshProcess> m_MeshProcess = nullptr;

		dtTileCache* m_TileCache = nullptr;
		dtNavMesh* m_NavMesh = nullptr;
		dtNavMeshQuery* m_NavQuery = nullptr;
		class RcContext : public rcContext
		{
		protected:
			void doLog(const rcLogCategory category, const char* msg, const int len) override
			{
				switch (category)
				{
					case RC_LOG_PROGRESS:
						EG_CORE_INFO("[AINavigation] {}", msg);
						break;
					case RC_LOG_WARNING:
						EG_CORE_WARN("[AINavigation] {}", msg);
						break;
					case RC_LOG_ERROR:
						EG_CORE_ERROR("[AINavigation] {}", msg);
						break;
				}
			}
		} m_Context;

		MeshSettings m_Settings;
		AgentType m_AgentType;
		rcConfig m_Config{};
		dtTileCacheParams m_TileCacheConfig{};
		dtQueryFilter m_Filter{};
		Crowd m_Crowd;
		int m_TilesX = 0; // Tile grid size
		int m_TilesZ = 0;
		bool m_bValid = false;
		bool m_bWarnedLargeObstacle = false;
		// Distinct tiles touched by obstacle requests queued since the tile cache was last up to date.
		// Detour's next batch is part of these, so keeping this within `GetMaxTilesPerBatch()` keeps the batch within it too
		std::vector<dtCompressedTileRef> m_BatchTiles;

		// Temp data for build process
		rcHeightfield* m_Solid = nullptr;
		rcCompactHeightfield* m_CompactHeightfield = nullptr;
		rcHeightfieldLayerSet* m_HeightfieldLayerSet = nullptr;
		rcChunkyTriMesh m_ChunkyMesh{};
		std::vector<int> m_CIDs;
		std::vector<uint8_t> m_TrianglesAreas;
		std::vector<NavigationTileData> m_Tiles;
		const float* m_Vertices = nullptr;
		int m_VertexCount = 0;
		const int* m_TriangleData = nullptr;
		int m_TriangleCount = 0;
	};
}
