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
		AABB AABB = { glm::vec3(-0.5f), glm::vec3(0.5f) };

		uint32_t MaxQueryNodes = 2048u;
		uint32_t ExpectedLayersPerTile = 4u;
		uint32_t MaxLayers = 32u;
		uint32_t MaxObstacles = 128u;
		uint32_t TileSize = 8u;
		float CellSize = 0.3f;
		float CellHeight = 0.001f;
		float MaxSlope = 45.f;
		float AgentHeight = 2.f;
		float AgentMaxClimb = 0.9f;
		float AgentRadius = 0.6f;
		float EdgeMaxLen = 12.f;
		float EdgeMaxError = 1.3f;
		float RegionMinSize = 8.f;
		float RegionMergeSize = 20.f;
		uint32_t VertsPerPoly = 6u;
		uint32_t BorderSize = 3u;

		bool FilterLowHangingObstacles = true;
		bool FilterLedgeSpans = true;
		bool FilterWalkableLowHeightSpans = true;
	};

	class Mesh
	{
	public:
		Mesh(const OverlapGeometryData& tiles, const MeshSettings& meshSettings, const CrowdSettings& crowdSettings);
		~Mesh();

		// Updates the tile cache by rebuilding tiles touched by unfinished obstacle requests.
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
		const AABB& GetAABB() const { return m_Settings.AABB; }

		Crowd& GetCrowd() { return m_Crowd; }
		const Crowd& GetCrowd() const { return m_Crowd; }

		static Ref<Mesh> Create(const OverlapGeometryData& tiles, const MeshSettings& settings, const CrowdSettings& crowdSettings);

	private:

		void InitConfig();
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
