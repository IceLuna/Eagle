#pragma once

#include "Eagle/Math/AABB.h"

#include <DetourAlloc.h>
#include <DetourStatus.h>
#include <DetourNavMeshQuery.h>
#include <DetourNavMeshBuilder.h>
#include <DetourTileCache.h>
#include <DetourTileCacheBuilder.h>

namespace Eagle::AINavigation
{
	inline constexpr uint32_t MaxAgentTypes = 8u;
	inline constexpr uint32_t InvalidAgentTypeID = 0xFFFFFFFFu;
	inline constexpr uint32_t AutoAgentTypeID = 0u;

	// Polygon areas are the project's areas, assigned by priority when a nav mesh is built (see `Mesh`). 1 is Default
	inline constexpr uint8_t DefaultRecastArea = 1u;

	// Recast stores a polygon's area in 6 bits. 0 isn't walkable, and 63 is Recast's walkable area before it's replaced by ours
	inline constexpr uint32_t MaxAreas = 62u;
	inline constexpr uint32_t DefaultAreaID = 0u; // Every walkable surface that isn't marked otherwise. Always exists
	inline constexpr uint32_t InvalidAreaID = 0xFFFFFFFFu;
	inline constexpr float MinAreaCost = 1.f; // Lower costs break the path search's distance estimate

	enum SamplePolyFlags
	{
		SAMPLE_POLYFLAGS_WALK = 0x01,		// Ability to walk (ground, grass, road)
		SAMPLE_POLYFLAGS_SWIM = 0x02,		// Ability to swim (water).
		SAMPLE_POLYFLAGS_DOOR = 0x04,		// Ability to move through doors.
		SAMPLE_POLYFLAGS_JUMP = 0x08,		// Ability to jump.
		SAMPLE_POLYFLAGS_DISABLED = 0x10,	// Disabled polygon
		SAMPLE_POLYFLAGS_ALL = 0xffff	    // All abilities.
	};

	// A kind of surface (mud, road, etc...) that's more or less expensive to walk on.
	// The path search counts a meter in an area as `Cost` meters
	struct Area
	{
		std::string Name;
		uint32_t ID = InvalidAreaID;
		glm::vec3 DebugColor = glm::vec3(0.6f, 0.4f, 0.2f);
		float Cost = 1.f;
		bool bBlocked = false; // Agents can't walk on it

		void Clamp() { Cost = glm::max(Cost, MinAreaCost); }
	};

	struct AreaVolume
	{
		glm::mat4 InverseTransform = glm::mat4(1.f); // From world space to the box's space
		glm::vec3 HalfExtents = glm::vec3(0.5f);
		AABB Bounds; // World space
		uint32_t AreaID = DefaultAreaID;
	};

	// Overrides an area's settings. Used by agents
	struct AreaOverride
	{
		uint32_t AreaID = InvalidAreaID;
		float Cost = 1.f;
		bool bBlocked = false;

		bool operator==(const AreaOverride&) const = default;

		void Clamp() { Cost = glm::max(Cost, MinAreaCost); }
	};

	// Used to block/unblock an area at runtime regardless of project settings
	struct AreaBlock
	{
		uint32_t AreaID = InvalidAreaID;
		bool bBlocked = false;
	};

	struct AreaCostSources
	{
		std::span<const Area> Areas; // Nav Areas by priority starting from the highest
		std::span<const AreaOverride> AgentTypeOverrides;
		std::span<const AreaBlock> BlockedAreas;
	};

	// Navigation data in binary Recast form.
	struct NavigationTileData
	{
		//! @returns true if the Recast data is not empty.
		bool IsValid() const
		{
			return m_size > 0 && m_data != nullptr;
		}

		unsigned char* m_data = nullptr;
		int m_size = 0;
	};

	struct TileCacheCompressor : public dtTileCacheCompressor
	{
		virtual ~TileCacheCompressor() {}

		int maxCompressedSize(const int bufferSize) override
		{
			return (int)(bufferSize * 1.05f);
		}

		dtStatus compress(const unsigned char* buffer, const int bufferSize,
			unsigned char* compressed, const int maxCompressedSize, int* compressedSize) override;

		dtStatus decompress(const unsigned char* compressed, const int compressedSize,
			unsigned char* buffer, const int maxBufferSize, int* bufferSize) override;
	};

	// Polygon flags of a Recast area
	inline uint16_t GetAreaPolyFlags(bool bBlocked) { return uint16_t(bBlocked ? (SAMPLE_POLYFLAGS_WALK | SAMPLE_POLYFLAGS_DISABLED) : SAMPLE_POLYFLAGS_WALK); }

	struct TileCacheMeshProcess : public dtTileCacheMeshProcess
	{
		// Blocked Recast areas. Owned by the nav mesh. Tiles are built again when obstacles change, so their flags come from the current state
		const std::array<bool, DT_MAX_AREAS>* BlockedAreas = nullptr;

		virtual void process(struct dtNavMeshCreateParams* params,
			unsigned char* polyAreas, unsigned short* polyFlags)
		{
			// Costs come from the query filters
			for (int i = 0; i < params->polyCount; ++i)
			{
				if (polyAreas[i] == DT_TILECACHE_WALKABLE_AREA)
					polyAreas[i] = DefaultRecastArea;

				const bool bBlocked = BlockedAreas && (*BlockedAreas)[polyAreas[i] % DT_MAX_AREAS];
				polyFlags[i] = GetAreaPolyFlags(bBlocked);
			}
		}
	};

	struct rcChunkyTriMeshNode
	{
		float bmin[2];
		float bmax[2];
		int i;
		int n;
	};

	struct rcChunkyTriMesh
	{
		inline rcChunkyTriMesh() : nodes(0), nnodes(0), tris(0), triIndices(0), ntris(0), maxTrisPerChunk(0) {}
		inline ~rcChunkyTriMesh() { Reset(); }

		void Reset()
		{
			delete[] nodes; delete[] tris; delete[] triIndices;
			nodes = nullptr;
			tris = nullptr;
			triIndices = nullptr;
		}

		rcChunkyTriMeshNode* nodes;
		int nnodes;
		int* tris;
		int* triIndices;
		int ntris;
		int maxTrisPerChunk;

	private:
		// Explicitly disabled copy constructor and copy assignment operator.
		rcChunkyTriMesh(const rcChunkyTriMesh&) = delete;
		rcChunkyTriMesh& operator=(const rcChunkyTriMesh&) = delete;
	};

	// Creates partitioned triangle mesh (AABB tree),
	// where each node contains at max trisPerChunk triangles.
	bool rcCreateChunkyTriMesh(const float* verts, const int* tris, int ntris,
		int trisPerChunk, rcChunkyTriMesh* cm);

	// Returns the chunk indices which overlap the input rectable.
	// outN - returns the number of found nodes.
	// It can be used by the called to resize `ids` accordingly so that it can be called again
	int rcGetChunksOverlappingRect(const rcChunkyTriMesh* cm, float bmin[2], float bmax[2], int* ids, const int maxIds, int* outN);

	// Returns the chunk indices which overlap the input segment.
	int rcGetChunksOverlappingSegment(const rcChunkyTriMesh* cm, float p[2], float q[2], int* ids, const int maxIds);

	inline bool InRange(const float* v1, const float* v2, const float r, const float h)
	{
		const float dx = v2[0] - v1[0];
		const float dy = v2[1] - v1[1];
		const float dz = v2[2] - v1[2];
		return (dx * dx + dz * dz) < r * r && fabsf(dy) < h;
	}

	bool GetSteerTarget(dtNavMeshQuery* navQuery, const float* startPos, const float* endPos,
		const float minTargetDist,
		const dtPolyRef* path, const int pathSize,
		float* steerPos, unsigned char& steerPosFlag, dtPolyRef& steerPosRef,
		float* outPoints = 0, int* outPointCount = 0);

	int dtMergeCorridorStartMoved(dtPolyRef* path, const int npath, const int maxPath,
		const dtPolyRef* visited, const int nvisited);

	// This function checks if the path has a small U-turn, that is,
	// a polygon further in the path is adjacent to the first polygon
	// in the path. If that happens, a shortcut is taken.
	// This can happen if the target (T) location is at tile boundary,
	// and we're (S) approaching it parallel to the tile edge.
	// The choice at the vertex can be arbitrary, 
	//  +---+---+
	//  |:::|:::|
	//  +-S-+-T-+
	//  |:::|   | <-- the step can end up in here, resulting U-turn path.
	//  +---+---+
	int FixupShortcuts(dtPolyRef* path, int npath, dtNavMeshQuery* navQuery);

	inline const Area* FindAreaOrDefault(std::span<const Area> areas, uint32_t areaID)
	{
		auto it = std::ranges::find_if(areas, [areaID](const Area& area) { return area.ID == areaID; });
		if (it == areas.end() && areaID != DefaultAreaID)
			it = std::ranges::find_if(areas, [](const Area& area) { return area.ID == DefaultAreaID; });

		return it != areas.end() ? &*it : nullptr;
	}

	inline const AreaOverride* FindAreaOverride(std::span<const AreaOverride> overrides, uint32_t areaID)
	{
		auto it = std::ranges::find_if(overrides, [areaID](const AreaOverride& areaOverride) { return areaOverride.AreaID == areaID; });
		return it != overrides.end() ? &*it : nullptr;
	}

	inline float ResolveAreaCost(const Area& area, std::span<const AreaOverride> overrides)
	{
		const AreaOverride* areaOverride = FindAreaOverride(overrides, area.ID);
		return glm::max(areaOverride ? areaOverride->Cost : area.Cost, MinAreaCost);
	}

	// The agent type's override decides it together with the cost. Without one, it's the area's own setting
	inline bool ResolveAreaBlocked(const Area& area, std::span<const AreaOverride> overrides)
	{
		const AreaOverride* areaOverride = FindAreaOverride(overrides, area.ID);
		return areaOverride ? areaOverride->bBlocked : area.bBlocked;
	}

	// Runtime blocking wins over the project settings
	inline bool ResolveAreaBlocked(const Area& area, const AreaCostSources& sources)
	{
		auto it = std::ranges::find_if(sources.BlockedAreas, [&area](const AreaBlock& block) { return block.AreaID == area.ID; });
		return it != sources.BlockedAreas.end() ? it->bBlocked : ResolveAreaBlocked(area, sources.AgentTypeOverrides);
	}
}
