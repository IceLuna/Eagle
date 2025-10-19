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
	// These are just sample areas to use consistent values across the samples.
	// The use should specify these base on his needs.
	enum SamplePolyAreas
	{
		SAMPLE_POLYAREA_GROUND,
		SAMPLE_POLYAREA_WATER,
		SAMPLE_POLYAREA_DOOR,
		SAMPLE_POLYAREA_JUMP
	};
	enum SamplePolyFlags
	{
		SAMPLE_POLYFLAGS_WALK = 0x01,		// Ability to walk (ground, grass, road)
		SAMPLE_POLYFLAGS_SWIM = 0x02,		// Ability to swim (water).
		SAMPLE_POLYFLAGS_DOOR = 0x04,		// Ability to move through doors.
		SAMPLE_POLYFLAGS_JUMP = 0x08,		// Ability to jump.
		SAMPLE_POLYFLAGS_DISABLED = 0x10,	// Disabled polygon
		SAMPLE_POLYFLAGS_ALL = 0xffff	    // All abilities.
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

	struct TileCacheMeshProcess : public dtTileCacheMeshProcess
	{
		virtual void process(struct dtNavMeshCreateParams* params,
			unsigned char* polyAreas, unsigned short* polyFlags)
		{
			// Update poly flags from areas.
			for (int i = 0; i < params->polyCount; ++i)
			{
				if (polyAreas[i] == DT_TILECACHE_WALKABLE_AREA)
					polyAreas[i] = SAMPLE_POLYAREA_GROUND;
		
				if (polyAreas[i] == SAMPLE_POLYAREA_GROUND)
				{
					polyFlags[i] = SAMPLE_POLYFLAGS_WALK;
				}
				else if (polyAreas[i] == SAMPLE_POLYAREA_WATER)
				{
					polyFlags[i] = SAMPLE_POLYFLAGS_SWIM;
				}
				else if (polyAreas[i] == SAMPLE_POLYAREA_DOOR)
				{
					polyFlags[i] = SAMPLE_POLYFLAGS_WALK | SAMPLE_POLYFLAGS_DOOR;
				}
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
		inline rcChunkyTriMesh() : nodes(0), nnodes(0), tris(0), ntris(0), maxTrisPerChunk(0) {}
		inline ~rcChunkyTriMesh() { Reset(); }

		void Reset()
		{
			delete[] nodes; delete[] tris;
			nodes = nullptr;
			tris = nullptr;
		}

		rcChunkyTriMeshNode* nodes;
		int nnodes;
		int* tris;
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
}
