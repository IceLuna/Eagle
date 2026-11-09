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

		AABB Bounds = { glm::vec3(-0.5f), glm::vec3(0.5f) };
		uint32_t MaxQueryNodes = 2048u;
		uint32_t ExpectedLayersPerTile = 4u;
		uint32_t MaxLayers = 32u;
		uint32_t MaxObstacles = 128u;
		uint32_t TileSize = 8u;
		float CellSize = 0.3f;
		float CellHeight = 0.1f;
		float MaxSlope = 45.f;
		float AgentHeight = 2.f;
		float AgentMaxClimb = 0.9f;
		float AgentRadius = 0.6f;
		float EdgeMaxError = 1.3f;
		uint32_t BorderSize = 3u;

		bool FilterLowHangingObstacles = true;
		bool FilterLedgeSpans = true;
		bool FilterWalkableLowHeightSpans = true;

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
			AgentHeight   = glm::max(AgentHeight, 0.001f);
			AgentMaxClimb = glm::max(AgentMaxClimb, 0.f);
			AgentRadius   = glm::max(AgentRadius, 0.f);
			EdgeMaxError  = glm::max(EdgeMaxError, 0.f);
			MaxSlope      = glm::clamp(MaxSlope, 0.f, 90.f);
		}
	};

	class Mesh
	{
	public:
		Mesh(const OverlapGeometryData& tiles, const MeshSettings& meshSettings, const CrowdSettings& crowdSettings);
		~Mesh();

		// Updates the tile cache by rebuilding tiles touched by unfinished obstacle requests (up to `MaxTileRebuildsPerUpdate`).
		// Also updates the crowd
		void Update(Timestep ts);

		dtObstacleRef AddCylinderObstacle(const glm::vec3& pos, float radius, float height);
		// Can be rotated around Y
		dtObstacleRef AddBoxObstacle(const glm::vec3& pos, const glm::vec3& halfExtents, float yRadians);
		bool RemoveObstacle(dtObstacleRef obstacle); // Returns true if success

		std::vector<glm::vec3> FindStraightPath(const glm::vec3& start, const glm::vec3& end, uint32_t maxPolys = 256u) const;
		std::vector<glm::vec3> FindSmoothPath(const glm::vec3& start, const glm::vec3& end, uint32_t maxPolys = 256u, uint32_t maxSmooth = 2048u) const;
		bool FindDistanceToWall(const glm::vec3& pos, float maxRadius, glm::vec3* outHitPos, glm::vec3* outHitNormal, float* outHitDistance);
		bool FindRandomPoint(glm::vec3* outRandomPoint);
		bool FindRandomPointInCircle(const glm::vec3& pos, float radius, glm::vec3* outRandomPoint);
		bool IsValidPoint(const glm::vec3& pos);

		void GetDebugDraw(duDebugDraw* debugDraw) const;
		const rcConfig& GetConfig() const { return m_Config; }
		const AABB& GetAABB() const { return m_Settings.Bounds; }

		Crowd& GetCrowd() { return m_Crowd; }
		const Crowd& GetCrowd() const { return m_Crowd; }

		// Returns null if the build failed
		static Ref<Mesh> Create(const OverlapGeometryData& tiles, const MeshSettings& settings, const CrowdSettings& crowdSettings);

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
