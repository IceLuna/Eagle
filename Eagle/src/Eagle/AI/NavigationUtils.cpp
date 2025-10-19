#include "egpch.h"
#include "NavigationUtils.h"

#include <DetourCommon.h>

namespace Eagle::AINavigation
{
	namespace Utils
	{
		struct BoundsItem
		{
			float bmin[2];
			float bmax[2];
			int i;
		};

		static bool CheckOverlapRect(const float amin[2], const float amax[2],
			const float bmin[2], const float bmax[2])
		{
			bool overlap = true;
			overlap = (amin[0] > bmax[0] || amax[0] < bmin[0]) ? false : overlap;
			overlap = (amin[1] > bmax[1] || amax[1] < bmin[1]) ? false : overlap;
			return overlap;
		}

		static bool CheckOverlapSegment(const float p[2], const float q[2],
			const float bmin[2], const float bmax[2])
		{
			static const float EPSILON = 1e-6f;

			float tmin = 0;
			float tmax = 1;
			float d[2];
			d[0] = q[0] - p[0];
			d[1] = q[1] - p[1];

			for (int i = 0; i < 2; i++)
			{
				if (fabsf(d[i]) < EPSILON)
				{
					// Ray is parallel to slab. No hit if origin not within slab
					if (p[i] < bmin[i] || p[i] > bmax[i])
						return false;
				}
				else
				{
					// Compute intersection t value of ray with near and far plane of slab
					float ood = 1.0f / d[i];
					float t1 = (bmin[i] - p[i]) * ood;
					float t2 = (bmax[i] - p[i]) * ood;
					if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
					if (t1 > tmin) tmin = t1;
					if (t2 < tmax) tmax = t2;
					if (tmin > tmax) return false;
				}
			}
			return true;
		}

		static int CompareItemX(const void* va, const void* vb)
		{
			const BoundsItem* a = (const BoundsItem*)va;
			const BoundsItem* b = (const BoundsItem*)vb;
			if (a->bmin[0] < b->bmin[0])
				return -1;
			if (a->bmin[0] > b->bmin[0])
				return 1;
			return 0;
		}

		static int CompareItemY(const void* va, const void* vb)
		{
			const BoundsItem* a = (const BoundsItem*)va;
			const BoundsItem* b = (const BoundsItem*)vb;
			if (a->bmin[1] < b->bmin[1])
				return -1;
			if (a->bmin[1] > b->bmin[1])
				return 1;
			return 0;
		}

		static void CalcExtends(const BoundsItem* items, const int /*nitems*/,
			const int imin, const int imax,
			float* bmin, float* bmax)
		{
			bmin[0] = items[imin].bmin[0];
			bmin[1] = items[imin].bmin[1];

			bmax[0] = items[imin].bmax[0];
			bmax[1] = items[imin].bmax[1];

			for (int i = imin + 1; i < imax; ++i)
			{
				const BoundsItem& it = items[i];
				if (it.bmin[0] < bmin[0]) bmin[0] = it.bmin[0];
				if (it.bmin[1] < bmin[1]) bmin[1] = it.bmin[1];

				if (it.bmax[0] > bmax[0]) bmax[0] = it.bmax[0];
				if (it.bmax[1] > bmax[1]) bmax[1] = it.bmax[1];
			}
		}

		static int LongestAxis(float x, float y)
		{
			return y > x ? 1 : 0;
		}

		static void Subdivide(BoundsItem* items, int nitems, int imin, int imax, int trisPerChunk,
			int& curNode, rcChunkyTriMeshNode* nodes, const int maxNodes,
			int& curTri, int* outTris, const int* inTris)
		{
			int inum = imax - imin;
			int icur = curNode;

			if (curNode >= maxNodes)
				return;

			rcChunkyTriMeshNode& node = nodes[curNode++];

			if (inum <= trisPerChunk)
			{
				// Leaf
				CalcExtends(items, nitems, imin, imax, node.bmin, node.bmax);

				// Copy triangles.
				node.i = curTri;
				node.n = inum;

				for (int i = imin; i < imax; ++i)
				{
					const int* src = &inTris[items[i].i * 3];
					int* dst = &outTris[curTri * 3];
					curTri++;
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
				}
			}
			else
			{
				// Split
				CalcExtends(items, nitems, imin, imax, node.bmin, node.bmax);

				int	axis = LongestAxis(node.bmax[0] - node.bmin[0],
					node.bmax[1] - node.bmin[1]);

				if (axis == 0)
				{
					// Sort along x-axis
					qsort(items + imin, static_cast<size_t>(inum), sizeof(BoundsItem), CompareItemX);
				}
				else if (axis == 1)
				{
					// Sort along y-axis
					qsort(items + imin, static_cast<size_t>(inum), sizeof(BoundsItem), CompareItemY);
				}

				int isplit = imin + inum / 2;

				// Left
				Subdivide(items, nitems, imin, isplit, trisPerChunk, curNode, nodes, maxNodes, curTri, outTris, inTris);
				// Right
				Subdivide(items, nitems, isplit, imax, trisPerChunk, curNode, nodes, maxNodes, curTri, outTris, inTris);

				int iescape = curNode - icur;
				// Negative index means escape.
				node.i = -iescape;
			}
		}
	}

	dtStatus TileCacheCompressor::compress(const unsigned char* buffer, const int bufferSize,
		unsigned char* compressed, const int maxCompressedSize, int* compressedSize)
	{
		EG_CORE_ASSERT(maxCompressedSize >= bufferSize);
		memcpy_s(compressed, maxCompressedSize, buffer, bufferSize);
		*compressedSize = bufferSize;
		return DT_SUCCESS;
	}

	dtStatus TileCacheCompressor::decompress(const unsigned char* compressed, const int compressedSize, unsigned char* buffer, const int maxBufferSize, int* bufferSize)
	{
		EG_CORE_ASSERT(maxBufferSize >= compressedSize);
		memcpy_s(buffer, maxBufferSize, compressed, compressedSize);
		*bufferSize = compressedSize;
		return DT_SUCCESS;
	}

	bool rcCreateChunkyTriMesh(const float* verts, const int* tris, int ntris, int trisPerChunk, rcChunkyTriMesh* cm)
	{
		int nchunks = (ntris + trisPerChunk - 1) / trisPerChunk;

		cm->nodes = new rcChunkyTriMeshNode[nchunks * 4];
		if (!cm->nodes)
			return false;

		cm->tris = new int[ntris * 3];
		if (!cm->tris)
			return false;

		cm->ntris = ntris;

		// Build tree
		Utils::BoundsItem* items = new Utils::BoundsItem[ntris];
		if (!items)
			return false;

		for (int i = 0; i < ntris; i++)
		{
			const int* t = &tris[i * 3];
			Utils::BoundsItem& it = items[i];
			it.i = i;
			// Calc triangle XZ bounds.
			it.bmin[0] = it.bmax[0] = verts[t[0] * 3 + 0];
			it.bmin[1] = it.bmax[1] = verts[t[0] * 3 + 2];
			for (int j = 1; j < 3; ++j)
			{
				const float* v = &verts[t[j] * 3];
				if (v[0] < it.bmin[0]) it.bmin[0] = v[0];
				if (v[2] < it.bmin[1]) it.bmin[1] = v[2];

				if (v[0] > it.bmax[0]) it.bmax[0] = v[0];
				if (v[2] > it.bmax[1]) it.bmax[1] = v[2];
			}
		}

		int curTri = 0;
		int curNode = 0;
		Utils::Subdivide(items, ntris, 0, ntris, trisPerChunk, curNode, cm->nodes, nchunks * 4, curTri, cm->tris, tris);

		delete[] items;

		cm->nnodes = curNode;

		// Calc max tris per node.
		cm->maxTrisPerChunk = 0;
		for (int i = 0; i < cm->nnodes; ++i)
		{
			rcChunkyTriMeshNode& node = cm->nodes[i];
			const bool isLeaf = node.i >= 0;
			if (!isLeaf) continue;
			if (node.n > cm->maxTrisPerChunk)
				cm->maxTrisPerChunk = node.n;
		}

		return true;
	}

	int rcGetChunksOverlappingRect(const rcChunkyTriMesh* cm, float bmin[2], float bmax[2], int* ids, const int maxIds, int* outN)
	{
		// Traverse tree
		int i = 0;
		int n = 0;
		*outN = 0;
		while (i < cm->nnodes)
		{
			const rcChunkyTriMeshNode* node = &cm->nodes[i];
			const bool overlap = Utils::CheckOverlapRect(bmin, bmax, node->bmin, node->bmax);
			const bool isLeafNode = node->i >= 0;

			if (isLeafNode && overlap)
			{
				if (n < maxIds)
				{
					ids[n] = i;
					n++;
				}
				++(*outN);
			}

			if (overlap || isLeafNode)
				i++;
			else
			{
				const int escapeIndex = -node->i;
				i += escapeIndex;
			}
		}

		return n;
	}

	int rcGetChunksOverlappingSegment(const rcChunkyTriMesh* cm, float p[2], float q[2], int* ids, const int maxIds)
	{
		// Traverse tree
		int i = 0;
		int n = 0;
		while (i < cm->nnodes)
		{
			const rcChunkyTriMeshNode* node = &cm->nodes[i];
			const bool overlap = Utils::CheckOverlapSegment(p, q, node->bmin, node->bmax);
			const bool isLeafNode = node->i >= 0;

			if (isLeafNode && overlap)
			{
				if (n < maxIds)
				{
					ids[n] = i;
					n++;
				}
			}

			if (overlap || isLeafNode)
				i++;
			else
			{
				const int escapeIndex = -node->i;
				i += escapeIndex;
			}
		}

		return n;
	}
	
	bool GetSteerTarget(dtNavMeshQuery* navQuery, const float* startPos, const float* endPos, const float minTargetDist, const dtPolyRef* path, const int pathSize, float* steerPos, unsigned char& steerPosFlag, dtPolyRef& steerPosRef, float* outPoints, int* outPointCount)
	{
		// Find steer target.
		static const int MAX_STEER_POINTS = 3;
		float steerPath[MAX_STEER_POINTS * 3];
		unsigned char steerPathFlags[MAX_STEER_POINTS];
		dtPolyRef steerPathPolys[MAX_STEER_POINTS];
		int nsteerPath = 0;
		navQuery->findStraightPath(startPos, endPos, path, pathSize,
			steerPath, steerPathFlags, steerPathPolys, &nsteerPath, MAX_STEER_POINTS);
		if (!nsteerPath)
			return false;

		if (outPoints && outPointCount)
		{
			*outPointCount = nsteerPath;
			for (int i = 0; i < nsteerPath; ++i)
				dtVcopy(&outPoints[i * 3], &steerPath[i * 3]);
		}


		// Find vertex far enough to steer to.
		int ns = 0;
		while (ns < nsteerPath)
		{
			// Stop at Off-Mesh link or when point is further than slop away.
			if ((steerPathFlags[ns] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ||
				!InRange(&steerPath[ns * 3], startPos, minTargetDist, 1000.0f))
				break;
			ns++;
		}
		// Failed to find good point to steer to.
		if (ns >= nsteerPath)
			return false;

		dtVcopy(steerPos, &steerPath[ns * 3]);
		steerPos[1] = startPos[1];
		steerPosFlag = steerPathFlags[ns];
		steerPosRef = steerPathPolys[ns];

		return true;
	}
	
	int dtMergeCorridorStartMoved(dtPolyRef* path, const int npath, const int maxPath, const dtPolyRef* visited, const int nvisited)
	{
		int furthestPath = -1;
		int furthestVisited = -1;

		// Find furthest common polygon.
		for (int i = npath - 1; i >= 0; --i)
		{
			bool found = false;
			for (int j = nvisited - 1; j >= 0; --j)
			{
				if (path[i] == visited[j])
				{
					furthestPath = i;
					furthestVisited = j;
					found = true;
				}
			}
			if (found)
				break;
		}

		// If no intersection found just return current path. 
		if (furthestPath == -1 || furthestVisited == -1)
			return npath;

		// Concatenate paths.	

		// Adjust beginning of the buffer to include the visited.
		const int req = nvisited - furthestVisited;
		const int orig = dtMin(furthestPath + 1, npath);
		int size = dtMax(0, npath - orig);
		if (req + size > maxPath)
			size = maxPath - req;
		if (size > 0)
			memmove(path + req, path + orig, size * sizeof(dtPolyRef));

		// Store visited
		for (int i = 0, n = dtMin(req, maxPath); i < n; ++i)
			path[i] = visited[(nvisited - 1) - i];

		return req + size;
	}
	
	int FixupShortcuts(dtPolyRef* path, int npath, dtNavMeshQuery* navQuery)
	{
		if (npath < 3)
			return npath;

		// Get connected polygons
		static const int maxNeis = 16;
		dtPolyRef neis[maxNeis];
		int nneis = 0;

		const dtMeshTile* tile = 0;
		const dtPoly* poly = 0;
		if (dtStatusFailed(navQuery->getAttachedNavMesh()->getTileAndPolyByRef(path[0], &tile, &poly)))
			return npath;

		for (unsigned int k = poly->firstLink; k != DT_NULL_LINK; k = tile->links[k].next)
		{
			const dtLink* link = &tile->links[k];
			if (link->ref != 0)
			{
				if (nneis < maxNeis)
					neis[nneis++] = link->ref;
			}
		}

		// If any of the neighbour polygons is within the next few polygons
		// in the path, short cut to that polygon directly.
		static const int maxLookAhead = 6;
		int cut = 0;
		for (int i = dtMin(maxLookAhead, npath) - 1; i > 1 && cut == 0; i--) {
			for (int j = 0; j < nneis; j++)
			{
				if (path[i] == neis[j]) {
					cut = i;
					break;
				}
			}
		}
		if (cut > 1)
		{
			int offset = cut - 1;
			npath -= offset;
			for (int i = 1; i < npath; i++)
				path[i] = path[i + offset];
		}

		return npath;
	}
}
