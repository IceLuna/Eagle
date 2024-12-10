#include "egpch.h"
#include "AINavigationMesh.h"
#include "DetourDebugDraw.h"
#include "RecastDebugDraw.h"
#include "DetourCommon.h"
#include "Eagle/Core/Random.h"
#include "Eagle/Physics/PhysicsEngine.h"

#include <glm/gtc/type_ptr.hpp>

namespace Eagle::AINavigation
{
	static uint32_t nextPow2(uint32_t v)
	{
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;
		return v;
	}

	static uint32_t log2(uint32_t v)
	{
		uint32_t r = (v > 0xffff) << 4; v >>= r;
		uint32_t shift = (v > 0xff) << 3; v >>= shift; r |= shift;
		shift = (v > 0xf) << 2; v >>= shift; r |= shift;
		shift = (v > 0x3) << 1; v >>= shift; r |= shift;
		r |= (v >> 1);
		return r;
	}

	static void drawTiles(duDebugDraw* dd, dtTileCache* tc)
	{
		unsigned int fcol[6];
		float bmin[3], bmax[3];

		for (int i = 0; i < tc->getTileCount(); ++i)
		{
			const dtCompressedTile* tile = tc->getTile(i);
			if (!tile->header) continue;

			tc->calcTightTileBounds(tile->header, bmin, bmax);

			const unsigned int col = duIntToCol(i, 64);
			duCalcBoxColors(fcol, col, col);
			duDebugDrawBox(dd, bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2], fcol);
		}
	}

	static void drawObstacles(duDebugDraw* dd, const dtTileCache* tc)
	{
		// Draw obstacles
		for (int i = 0; i < tc->getObstacleCount(); ++i)
		{
			const dtTileCacheObstacle* ob = tc->getObstacle(i);
			if (ob->state == DT_OBSTACLE_EMPTY) continue;
			float bmin[3], bmax[3];
			tc->getObstacleBounds(ob, bmin, bmax);

			uint32_t col = 0;
			if (ob->state == DT_OBSTACLE_PROCESSING)
				col = duRGBA(255, 255, 0, 255);
			else if (ob->state == DT_OBSTACLE_PROCESSED)
				col = duRGBA(0, 255, 0, 192);
			else if (ob->state == DT_OBSTACLE_REMOVING)
				col = duRGBA(255, 0, 0, 128);

			switch (ob->type)
			{
				case DT_OBSTACLE_CYLINDER:
				{
					duDebugDrawCylinder(dd, bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2], col);
					duDebugDrawCylinderWire(dd, bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2], duDarkenCol(col), 2);
					break;
				}
				case DT_OBSTACLE_BOX:
				case DT_OBSTACLE_ORIENTED_BOX:
				{
					uint32_t col6[6];
					for (uint32_t i = 0; i < 6; ++i)
						col6[i] = col;
					duDebugDrawBox(dd, bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2], col6);
					duDebugDrawBoxWire(dd, bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2], duDarkenCol(col), 2);
					break;
				}
			}
		}
	}

	Mesh::Mesh(const OverlapGeometryData& geometry, const MeshSettings& meshSettings, const CrowdSettings& crowdSettings)
		: m_Settings(meshSettings)
	{
		m_Filter.setIncludeFlags(SAMPLE_POLYFLAGS_ALL ^ SAMPLE_POLYFLAGS_DISABLED);
		m_Filter.setExcludeFlags(0);
		m_Filter.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
		m_Filter.setAreaCost(SAMPLE_POLYAREA_WATER, 10.0f);
		m_Filter.setAreaCost(SAMPLE_POLYAREA_DOOR, 1.0f);
		m_Filter.setAreaCost(SAMPLE_POLYAREA_JUMP, 1.5f);

		m_Compressor = MakeScope<TileCacheCompressor>();
		m_MeshProcess = MakeScope<TileCacheMeshProcess>();

		InitConfig();
		if (!CreateTileCache())
			return;
		if (!CreateNavigationMesh())
			return;

		m_Vertices = geometry.Vertices.empty() ? nullptr : &geometry.Vertices.front()[0];
		m_VertexCount = static_cast<int>(geometry.Vertices.size());
		m_TriangleData = (int*)(geometry.Indices.empty() ? nullptr : &geometry.Indices[0]);
		m_TriangleCount = static_cast<int>(geometry.Indices.size()) / 3;

		m_ChunkyMesh.Reset();
		if (!rcCreateChunkyTriMesh(m_Vertices, m_TriangleData, m_TriangleCount, 256, &m_ChunkyMesh))
		{
			EG_CORE_ERROR("[AINavigation] Failed to build chunky mesh");
		}

		const float* bmin = glm::value_ptr(m_Settings.AABB.Min);
		const float* bmax = glm::value_ptr(m_Settings.AABB.Max);

		int gw = 0, gh = 0;
		rcCalcGridSize(bmin, bmax, m_Settings.CellSize, &gw, &gh);
		const int ts = (int)m_Settings.TileSize;
		const int tw = (gw + ts - 1) / ts;
		const int th = (gh + ts - 1) / ts;

		{
			const uint32_t initialSize = 4096;
			m_CIDs.resize(initialSize);
			m_Tiles.resize(m_Settings.MaxLayers);
		}

		for (int y = 0; y < th; ++y)
		{
			for (int x = 0; x < tw; ++x)
			{
				if (!RasterizeInputPolygonSoup(x, y))
				{
					continue;
				}

				// Once all geometry is rasterized, we do initial pass of filtering to
				// remove unwanted overhangs caused by the conservative rasterization
				// as well as filter spans where the character cannot possibly stand.
				if (m_Settings.FilterLowHangingObstacles)
				{
					rcFilterLowHangingWalkableObstacles(&m_Context, m_Config.walkableClimb, *m_Solid);
				}
				if (m_Settings.FilterLedgeSpans)
				{
					rcFilterLedgeSpans(&m_Context, m_Config.walkableHeight, m_Config.walkableClimb, *m_Solid);
				}
				if (m_Settings.FilterWalkableLowHeightSpans)
				{
					rcFilterWalkableLowHeightSpans(&m_Context, m_Config.walkableHeight, *m_Solid);
				}

				if (!PartitionWalkableSurfaceToSimpleRegions(x, y))
				{
					continue;
				}
			}
		}

		for (int y = 0; y < th; ++y)
		{
			for (int x = 0; x < tw; ++x)
			{
				m_TileCache->buildNavMeshTilesAt(x, y, m_NavMesh);
			}
		}
	
		m_Crowd.Init(crowdSettings, m_NavMesh, m_NavQuery, m_Filter);
	}

	Mesh::~Mesh()
	{
		m_Crowd.Release();

		dtFreeNavMesh(m_NavMesh); 
		dtFreeNavMeshQuery(m_NavQuery);
		dtFreeTileCache(m_TileCache);
		rcFreeHeightField(m_Solid);
		rcFreeCompactHeightfield(m_CompactHeightfield);
		rcFreeHeightfieldLayerSet(m_HeightfieldLayerSet);

		m_Allocator.reset();
		m_Compressor.reset();
		m_MeshProcess.reset();

		m_NavMesh = nullptr;
		m_NavQuery = nullptr;
		m_TileCache = nullptr;
		m_Solid = nullptr;
		m_CompactHeightfield = nullptr;
		m_HeightfieldLayerSet = nullptr;
	}

	void Mesh::Update(Timestep ts)
	{
		m_TileCache->update(ts, m_NavMesh);
		m_Crowd.Update(ts);
	}

	dtObstacleRef Mesh::AddCylinderObstacle(const glm::vec3& pos, float radius, float height)
	{
		dtObstacleRef obstacle = 0;
		const dtStatus status = m_TileCache->addObstacle(&pos[0], radius, height, &obstacle);
		if (dtStatusFailed(status))
		{
			EG_CORE_ERROR("[AINavigation] Failed to add a cylinder obstacle. Error code: {}", status);
		}
		return obstacle;
	}

	dtObstacleRef Mesh::AddBoxObstacle(const glm::vec3& pos, const glm::vec3& halfExtents, float yRadians)
	{
		dtObstacleRef obstacle = 0;
		const dtStatus status = m_TileCache->addBoxObstacle(&pos[0], &halfExtents[0], yRadians, &obstacle);
		if (dtStatusFailed(status))
		{
			EG_CORE_ERROR("[AINavigation] Failed to add a box obstacle. Error code: {}", status);
		}
		return obstacle;
	}

	bool Mesh::RemoveObstacle(dtObstacleRef obstacle)
	{
		const dtStatus status = m_TileCache->removeObstacle(obstacle);
		const bool bSuccess = dtStatusSucceed(status);
		if (!bSuccess)
		{
			EG_CORE_ERROR("[AINavigation] Failed to remove an obstacle. Error code: {}", status);
		}
		return bSuccess;
	}

	std::vector<glm::vec3> Mesh::FindStraightPath(const glm::vec3& start, const glm::vec3& end, uint32_t maxPolys) const
	{
		const float searchHalfExtent[3] = { 2, 4, 2 };
		dtPolyRef startPoly, endPoly;
		m_NavQuery->findNearestPoly(&start[0], searchHalfExtent, &m_Filter, &startPoly, nullptr);
		m_NavQuery->findNearestPoly(&end[0], searchHalfExtent, &m_Filter, &endPoly, nullptr);
		std::vector<dtPolyRef> polys(maxPolys);
		int npolys = 0;
		m_NavQuery->findPath(startPoly, endPoly, &start[0], &end[0], &m_Filter, polys.data(), &npolys, maxPolys);

		if (npolys == 0)
		{
			return {};
		}

		// In case of partial path, make sure the end point is clamped to the last polygon.
		float epos[3];
		dtVcopy(epos, &end[0]);
		if (polys[npolys - 1] != endPoly)
			m_NavQuery->closestPointOnPoly(polys[npolys - 1], &end[0], epos, 0);

		std::vector<glm::vec3> straightPath(maxPolys);
		std::vector<unsigned char> straightPathFlags(maxPolys);
		std::vector<dtPolyRef> straightPathPolys(maxPolys);
		int nstraightPath;
		int straightPathOptions = 0;

		m_NavQuery->findStraightPath(&start[0], epos, polys.data(), npolys,
			&straightPath[0].x, straightPathFlags.data(),
			straightPathPolys.data(), &nstraightPath, maxPolys, straightPathOptions);

		straightPath.resize(nstraightPath);
		return straightPath;
	}

	std::vector<glm::vec3> Mesh::FindSmoothPath(const glm::vec3& start, const glm::vec3& end, uint32_t maxPolys, uint32_t maxSmooth) const
	{
		const float searchHalfExtent[3] = { 2, 4, 2 }; // TODO: Expose
		dtPolyRef startPoly, endPoly;
		m_NavQuery->findNearestPoly(&start[0], searchHalfExtent, &m_Filter, &startPoly, nullptr);
		m_NavQuery->findNearestPoly(&end[0], searchHalfExtent, &m_Filter, &endPoly, nullptr);
		std::vector<dtPolyRef> polys(maxPolys);
		int npolys = 0;
		m_NavQuery->findPath(startPoly, endPoly, &start[0], &end[0], &m_Filter, polys.data(), &npolys, maxPolys);

		if (npolys == 0)
		{
			return {};
		}

		std::vector<glm::vec3> smoothPath(maxSmooth);
		int nsmoothPath = 0;

		// Iterate over the path to find smooth path on the detail mesh surface.
		float iterPos[3], targetPos[3];
		m_NavQuery->closestPointOnPoly(startPoly, &start[0], iterPos, 0);
		m_NavQuery->closestPointOnPoly(polys[npolys - 1], &end[0], targetPos, 0);

		static const float STEP_SIZE = 0.5f;
		static const float SLOP = 0.01f;

		nsmoothPath = 0;

		dtVcopy(&smoothPath[nsmoothPath].x, iterPos);
		nsmoothPath++;

		// Move towards target a small advancement at a time until target reached or
		// when ran out of memory to store the path.
		while (npolys && nsmoothPath < (int)maxPolys)
		{
			// Find location to steer towards.
			float steerPos[3];
			unsigned char steerPosFlag;
			dtPolyRef steerPosRef;

			if (!GetSteerTarget(m_NavQuery, iterPos, targetPos, SLOP, polys.data(), npolys, steerPos, steerPosFlag, steerPosRef))
				break;

			bool endOfPath = (steerPosFlag & DT_STRAIGHTPATH_END) ? true : false;
			bool offMeshConnection = (steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ? true : false;

			// Find movement delta.
			float delta[3], len;
			dtVsub(delta, steerPos, iterPos);
			len = dtMathSqrtf(dtVdot(delta, delta));
			// If the steer target is end of path or off-mesh link, do not move past the location.
			if ((endOfPath || offMeshConnection) && len < STEP_SIZE)
				len = 1;
			else
				len = STEP_SIZE / len;
			float moveTgt[3];
			dtVmad(moveTgt, iterPos, delta, len);

			// Move
			float result[3];
			dtPolyRef visited[16];
			int nvisited = 0;
			m_NavQuery->moveAlongSurface(polys[0], iterPos, moveTgt, &m_Filter,
				result, visited, &nvisited, 16);

			npolys = dtMergeCorridorStartMoved(polys.data(), npolys, maxPolys, visited, nvisited);
			npolys = FixupShortcuts(polys.data(), npolys, m_NavQuery);

			float h = 0;
			m_NavQuery->getPolyHeight(polys[0], result, &h);
			result[1] = h;
			dtVcopy(iterPos, result);

			// Handle end of path and off-mesh links when close enough.
			if (endOfPath && InRange(iterPos, steerPos, SLOP, 1.0f))
			{
				// Reached end of path.
				dtVcopy(iterPos, targetPos);
				if (nsmoothPath < (int)maxSmooth)
				{
					dtVcopy(&smoothPath[nsmoothPath].x, iterPos);
					nsmoothPath++;
				}
				break;
			}
			else if (offMeshConnection && InRange(iterPos, steerPos, SLOP, 1.0f))
			{
				// Reached off-mesh connection.
				float startPos[3], endPos[3];

				// Advance the path up to and over the off-mesh connection.
				dtPolyRef prevRef = 0, polyRef = polys[0];
				int npos = 0;
				while (npos < npolys && polyRef != steerPosRef)
				{
					prevRef = polyRef;
					polyRef = polys[npos];
					npos++;
				}
				for (int i = npos; i < npolys; ++i)
					polys[i - npos] = polys[i];
				npolys -= npos;

				// Handle the connection.
				dtStatus status = m_NavMesh->getOffMeshConnectionPolyEndPoints(prevRef, polyRef, startPos, endPos);
				if (dtStatusSucceed(status))
				{
					if (nsmoothPath < (int)maxSmooth)
					{
						dtVcopy(&smoothPath[nsmoothPath].x, startPos);
						nsmoothPath++;
						// Hack to make the dotted path not visible during off-mesh connection.
						if (nsmoothPath & 1)
						{
							dtVcopy(&smoothPath[nsmoothPath].x, startPos);
							nsmoothPath++;
						}
					}
					// Move position at the other side of the off-mesh link.
					dtVcopy(iterPos, endPos);
					float eh = 0.0f;
					m_NavQuery->getPolyHeight(polys[0], iterPos, &eh);
					iterPos[1] = eh;
				}
			}

			// Store results.
			if (nsmoothPath < (int)maxSmooth)
			{
				dtVcopy(&smoothPath[nsmoothPath].x, iterPos);
				nsmoothPath++;
			}
		}
	
		smoothPath.resize(nsmoothPath);
		return smoothPath;
	}

	bool Mesh::FindDistanceToWall(const glm::vec3& pos, float maxRadius, glm::vec3* outHitPos, glm::vec3* outHitNormal, float* outHitDistance)
	{
		const float searchHalfExtent[3] = { 2, 4, 2 };
		dtPolyRef poly;
		m_NavQuery->findNearestPoly(&pos[0], searchHalfExtent, &m_Filter, &poly, nullptr);

		const dtStatus status = m_NavQuery->findDistanceToWall(poly, &pos[0], maxRadius, &m_Filter, outHitDistance, &outHitPos->x, &outHitNormal->x);
		return dtStatusSucceed(status);
	}

	bool Mesh::FindRandomPoint(glm::vec3* outRandomPoint)
	{
		dtPolyRef randomPoly;
		const dtStatus status = m_NavQuery->findRandomPoint(&m_Filter, Random::Float, &randomPoly, &outRandomPoint->x);
		return dtStatusSucceed(status);
	}

	bool Mesh::FindRandomPointInCircle(const glm::vec3& pos, float radius, glm::vec3* outRandomPoint)
	{
		const float searchHalfExtent[3] = { 2, 4, 2 };
		dtPolyRef poly;
		m_NavQuery->findNearestPoly(&pos[0], searchHalfExtent, &m_Filter, &poly, nullptr);

		dtPolyRef randomPoly;
		const dtStatus status = m_NavQuery->findRandomPointAroundCircle(poly, &pos.x, radius, &m_Filter, Random::Float, &randomPoly, &outRandomPoint->x);
		return dtStatusSucceed(status);
	}

	bool Mesh::IsValidPoint(const glm::vec3& pos)
	{
		const float searchHalfExtent[3] = { 2, 4, 2 };
		dtPolyRef poly;
		m_NavQuery->findNearestPoly(&pos[0], searchHalfExtent, &m_Filter, &poly, nullptr);
		return m_NavQuery->isValidPolyRef(poly, &m_Filter);
	}

	void Mesh::GetDebugDraw(duDebugDraw* debugDraw) const
	{
		constexpr uint8_t drawFlags = DU_DRAWNAVMESH_OFFMESHCONS | DU_DRAWNAVMESH_CLOSEDLIST | DU_DRAWNAVMESH_COLOR_TILES;

		//drawTiles(debugDraw, m_TileCache);
		//drawObstacles(debugDraw, m_TileCache);
		duDebugDrawNavMeshWithClosedList(debugDraw, *m_NavMesh, *m_NavQuery, drawFlags/*|DU_DRAWNAVMESH_COLOR_TILES*/);
	}

	Ref<Mesh> Mesh::Create(const OverlapGeometryData& geometry, const MeshSettings& settings, const CrowdSettings& crowdSettings)
	{
		return MakeRef<Mesh>(geometry, settings, crowdSettings);
	}

	void Mesh::InitConfig()
	{
		const float* bmin = glm::value_ptr(m_Settings.AABB.Min);
		const float* bmax = glm::value_ptr(m_Settings.AABB.Max);

		{
			constexpr float DetailSampleDist = 6.f;
			constexpr float DetailSampleMaxError = 1.f;

			rcConfig& cfg = m_Config;
			memset(&cfg, 0, sizeof(cfg));
			cfg.cs = m_Settings.CellSize;
			cfg.ch = m_Settings.CellHeight;
			cfg.walkableSlopeAngle = m_Settings.MaxSlope;
			cfg.walkableHeight = glm::min(3, (int)ceilf(m_Settings.AgentHeight / cfg.ch));
			cfg.walkableClimb = (int)floorf(m_Settings.AgentMaxClimb / cfg.ch);
			cfg.walkableRadius = (int)ceilf(m_Settings.AgentRadius / cfg.cs);
			cfg.maxEdgeLen = (int)(m_Settings.EdgeMaxLen / m_Settings.CellSize);
			cfg.maxSimplificationError = m_Settings.EdgeMaxError;
			cfg.minRegionArea = (int)rcSqr(m_Settings.RegionMinSize);		// Note: area = size*size
			cfg.mergeRegionArea = (int)rcSqr(m_Settings.RegionMergeSize);	// Note: area = size*size
			cfg.maxVertsPerPoly = (int)m_Settings.VertsPerPoly;
			cfg.tileSize = (int)m_Settings.TileSize;
			cfg.borderSize = cfg.walkableRadius + (int)m_Settings.BorderSize; // Reserve enough padding.
			cfg.detailSampleDist = DetailSampleDist < 0.9f ? 0 : m_Settings.CellSize * DetailSampleDist;
			cfg.detailSampleMaxError = m_Settings.CellHeight * DetailSampleMaxError;
			rcVcopy(cfg.bmin, bmin);
			rcVcopy(cfg.bmax, bmax);
			rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);
		}

		// Tile cache params.
		{
			int gw = 0, gh = 0;
			rcCalcGridSize(bmin, bmax, m_Settings.CellSize, &gw, &gh);
			const int ts = (int)m_Settings.TileSize;
			const int tw = (gw + ts - 1) / ts;
			const int th = (gh + ts - 1) / ts;

			dtTileCacheParams& tcparams = m_TileCacheConfig;
			memset(&tcparams, 0, sizeof(tcparams));
			rcVcopy(tcparams.orig, bmin);
			tcparams.cs = m_Settings.CellSize;
			tcparams.ch = m_Settings.CellHeight;
			tcparams.width = (int)m_Settings.TileSize;
			tcparams.height = (int)m_Settings.TileSize;
			tcparams.walkableHeight = m_Settings.AgentHeight;
			tcparams.walkableRadius = m_Settings.AgentRadius;
			tcparams.walkableClimb = m_Settings.AgentMaxClimb;
			tcparams.maxSimplificationError = m_Settings.EdgeMaxError;
			tcparams.maxTiles = tw * th * m_Settings.ExpectedLayersPerTile;
			tcparams.maxObstacles = m_Settings.MaxObstacles;
		}
	}

	bool Mesh::CreateTileCache()
	{
		dtFreeTileCache(m_TileCache);
		m_TileCache = dtAllocTileCache();
		if (!m_TileCache)
		{
			EG_CORE_ERROR("[AINavigation] Failed to allocate TileCache");
			return false;
		}

		auto status = m_TileCache->init(&m_TileCacheConfig, &m_Allocator, m_Compressor.get(), m_MeshProcess.get());
		if (dtStatusFailed(status))
		{
			EG_CORE_ERROR("[AINavigation] Failed to init TileCache");
			return false;
		}

		return true;
	}

	bool Mesh::CreateNavigationMesh()
	{
		dtFreeNavMesh(m_NavMesh);
		m_NavMesh = dtAllocNavMesh();
		if (!m_NavMesh)
		{
			EG_CORE_ERROR("[AINavigation] Failed to allocate NavMesh");
			return false;
		}

		const float* bmin = m_Config.bmin;
		const float* bmax = m_Config.bmax;

		int gw = 0, gh = 0;
		rcCalcGridSize(bmin, bmax, m_Settings.CellSize, &gw, &gh);
		const int ts = (int)m_Settings.TileSize;
		const int tw = (gw + ts - 1) / ts;
		const int th = (gh + ts - 1) / ts;

		// Max tiles and max polys affect how the tile IDs are caculated.
		// There are 22 bits available for identifying a tile and a polygon.
		int tileBits = rcMin((int)log2(nextPow2(tw * th * m_Settings.ExpectedLayersPerTile)), 14);
		if (tileBits > 14) tileBits = 14;
		int polyBits = 22 - tileBits;

		dtNavMeshParams params;
		memset(&params, 0, sizeof(params));
		rcVcopy(params.orig, bmin);
		params.tileWidth = m_Settings.TileSize * m_Settings.CellSize;
		params.tileHeight = m_Settings.TileSize * m_Settings.CellSize;
		params.maxTiles = 1 << tileBits;
		params.maxPolys = 1 << polyBits;

		dtStatus status = m_NavMesh->init(&params);
		if (dtStatusFailed(status))
		{
			EG_CORE_ERROR("[AINavigation] Failed to init NavMesh");
			return false;
		}

		dtFreeNavMeshQuery(m_NavQuery);
		m_NavQuery = dtAllocNavMeshQuery();

		status = m_NavQuery->init(m_NavMesh, m_Settings.MaxQueryNodes);
		if (dtStatusFailed(status))
		{
			EG_CORE_ERROR("[AINavigation] Failed to init NavMesh query");
			return false;
		}

		return true;
	}

	bool Mesh::RasterizeInputPolygonSoup(int x, int y)
	{
		// Tile bounds.
		const auto& cfg = m_Config;
		const float tcs = cfg.tileSize * cfg.cs;

		rcConfig tcfg;
		memcpy(&tcfg, &cfg, sizeof(tcfg));

		tcfg.bmin[0] = cfg.bmin[0] + x * tcs;
		tcfg.bmin[1] = cfg.bmin[1];
		tcfg.bmin[2] = cfg.bmin[2] + y * tcs;
		tcfg.bmax[0] = cfg.bmin[0] + (x + 1) * tcs;
		tcfg.bmax[1] = cfg.bmax[1];
		tcfg.bmax[2] = cfg.bmin[2] + (y + 1) * tcs;

		tcfg.bmax[0] = glm::min(tcfg.bmax[0], cfg.bmax[0]);
		tcfg.bmax[2] = glm::min(tcfg.bmax[2], cfg.bmax[2]);

		tcfg.bmin[0] -= tcfg.borderSize * tcfg.cs;
		tcfg.bmin[2] -= tcfg.borderSize * tcfg.cs;
		tcfg.bmax[0] += tcfg.borderSize * tcfg.cs;
		tcfg.bmax[2] += tcfg.borderSize * tcfg.cs;

		// Recalculate width/height for the potentially limited tcfg bmin/bmax
		rcCalcGridSize(tcfg.bmin, tcfg.bmax, tcfg.cs, &tcfg.width, &tcfg.height);
		
		// Allocate voxel height field where we rasterize our input data to.
		rcFreeHeightField(m_Solid);
		m_Solid = rcAllocHeightfield();
		if (!m_Solid)
		{
			EG_CORE_ERROR("[AINavigation] Failed to allocate Heightfield");
			return false;
		}
		if (!rcCreateHeightfield(&m_Context, *m_Solid, tcfg.width, tcfg.height,
			tcfg.bmin, tcfg.bmax, tcfg.cs, tcfg.ch))
		{
			EG_CORE_ERROR("[AINavigation] Failed to create Heightfield");
			return false;
		}

		float tbmin[2] = { tcfg.bmin[0], tcfg.bmin[2] };
		float tbmax[2] = { tcfg.bmax[0], tcfg.bmax[2] };

		int maxIDs = 0;
		int ncid = rcGetChunksOverlappingRect(&m_ChunkyMesh, tbmin, tbmax, m_CIDs.data(), (int)m_CIDs.size(), &maxIDs);
		if (maxIDs > (int)m_CIDs.size())
		{
			m_CIDs.resize(maxIDs);
			ncid = rcGetChunksOverlappingRect(&m_ChunkyMesh, tbmin, tbmax, m_CIDs.data(), (int)m_CIDs.size(), &maxIDs);
		}

		if (!ncid)
		{
			return false;
		}

		for (int i = 0; i < ncid; ++i)
		{
			const rcChunkyTriMeshNode& node = m_ChunkyMesh.nodes[m_CIDs[i]];
			const int* tris = &m_ChunkyMesh.tris[node.i * 3];
			const int ntris = node.n;

			if (m_TrianglesAreas.size() < ntris)
			{
				m_TrianglesAreas.resize(ntris);
			}
			std::fill(m_TrianglesAreas.begin(), m_TrianglesAreas.end(), 0);
			rcMarkWalkableTriangles(&m_Context, tcfg.walkableSlopeAngle,
				m_Vertices, m_VertexCount, tris, ntris, m_TrianglesAreas.data());

			if (!rcRasterizeTriangles(&m_Context, m_Vertices, m_VertexCount, tris, m_TrianglesAreas.data(), ntris, *m_Solid, tcfg.walkableClimb))
			{
				EG_CORE_ERROR("[AINavigation] Failed to rasterize triangles at tile {}x{}, ID: {}", x, y, i);
			}
		}

		return true;
	}

	bool Mesh::PartitionWalkableSurfaceToSimpleRegions(int x, int y)
	{
		// Compact the height field so that it is faster to handle from now on.
		// This will result more cache coherent data as well as the neighbors
		// between walkable cells will be calculated.
		rcFreeCompactHeightfield(m_CompactHeightfield);
		m_CompactHeightfield = rcAllocCompactHeightfield();
		if (!m_CompactHeightfield)
		{
			EG_CORE_ERROR("[AINavigation] Failed to allocate CompactHeightfield");
			return false;
		}
		if (!rcBuildCompactHeightfield(&m_Context, m_Config.walkableHeight, m_Config.walkableClimb, *m_Solid, *m_CompactHeightfield))
		{
			EG_CORE_ERROR("[AINavigation] Failed to build CompactHeightfield");
			return false;
		}

		rcFreeHeightField(m_Solid);
		m_Solid = nullptr;

		// Erode the walkable area by agent radius.
		if (!rcErodeWalkableArea(&m_Context, m_Config.walkableRadius, *m_CompactHeightfield))
		{
			EG_CORE_ERROR("[AINavigation] Failed to erode Walkable Area");
			return false;
		}

		rcFreeHeightfieldLayerSet(m_HeightfieldLayerSet);
		m_HeightfieldLayerSet = rcAllocHeightfieldLayerSet();
		if (!m_HeightfieldLayerSet)
		{
			EG_CORE_ERROR("[AINavigation] Failed to allocate Heightfield Layer Set");
			return false;
		}
		if (!rcBuildHeightfieldLayers(&m_Context, *m_CompactHeightfield, m_Config.borderSize, m_Config.walkableHeight, *m_HeightfieldLayerSet))
		{
			EG_CORE_ERROR("[AINavigation] Failed to build Heightfield Layers");
			return false;
		}
		rcFreeCompactHeightfield(m_CompactHeightfield);
		m_CompactHeightfield = nullptr;

		int ntiles = 0;
		for (auto& tile : m_Tiles)
			tile = NavigationTileData{};

		for (int i = 0; i < rcMin(m_HeightfieldLayerSet->nlayers, (int)m_Settings.MaxLayers); ++i)
		{
			NavigationTileData& tile = m_Tiles[ntiles++];
			const rcHeightfieldLayer* layer = &m_HeightfieldLayerSet->layers[i];

			// Store header
			dtTileCacheLayerHeader header;
			header.magic = DT_TILECACHE_MAGIC;
			header.version = DT_TILECACHE_VERSION;

			// Tile layer location in the navmesh.
			header.tx = x;
			header.ty = y;
			header.tlayer = i;
			dtVcopy(header.bmin, layer->bmin);
			dtVcopy(header.bmax, layer->bmax);

			// Tile info.
			header.width = (unsigned char)layer->width;
			header.height = (unsigned char)layer->height;
			header.minx = (unsigned char)layer->minx;
			header.maxx = (unsigned char)layer->maxx;
			header.miny = (unsigned char)layer->miny;
			header.maxy = (unsigned char)layer->maxy;
			header.hmin = (unsigned short)layer->hmin;
			header.hmax = (unsigned short)layer->hmax;

			dtStatus status = dtBuildTileCacheLayer(m_Compressor.get(), &header, layer->heights, layer->areas, layer->cons,
				&tile.m_data, &tile.m_size);
			if (dtStatusFailed(status))
			{
				EG_CORE_ERROR("[AINavigation] Failed to build TileCache layer");
				continue;
			}
		}
		rcFreeHeightfieldLayerSet(m_HeightfieldLayerSet);
		m_HeightfieldLayerSet = nullptr;

		for (int i = 0; i < ntiles; ++i)
		{
			NavigationTileData& tile = m_Tiles[i];
			if (!tile.m_data)
				continue;

			auto status = m_TileCache->addTile(tile.m_data, tile.m_size, DT_COMPRESSEDTILE_FREE_DATA, 0);
			if (dtStatusFailed(status))
			{
				EG_CORE_ERROR("[AINavigation] Failed to add tile");
				dtFree(tile.m_data);
				tile.m_size = 0;
			}
		}

		return true;
	}

	bool Mesh::AttachNavigationTileToMesh(NavigationTileData& tileData)
	{
		dtTileRef tileRef = 0;
		const dtStatus status = m_NavMesh->addTile(tileData.m_data, tileData.m_size, DT_TILE_FREE_DATA, 0, &tileRef);
		if (dtStatusFailed(status))
		{
			EG_CORE_ERROR("[AINavigation] Failed to add tile");
			dtFree(tileData.m_data);
			return false;
		}

		return true;
	}
}
