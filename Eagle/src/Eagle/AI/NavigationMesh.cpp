#include "egpch.h"
#include "NavigationMesh.h"
#include "DetourDebugDraw.h"
#include "RecastDebugDraw.h"
#include "DetourCommon.h"
#include "Eagle/Core/Application.h"
#include "Eagle/Core/Random.h"
#include "Eagle/Physics/PhysicsEngine.h"

#include <glm/gtc/type_ptr.hpp>

namespace Eagle::AINavigation
{
	// Finds the nav mesh point that lets an agent occupy a position (the nearest one horizontally among the points within the agent's space)
	class AgentSpaceQuery : public dtPolyQuery
	{
	public:
		AgentSpaceQuery(const dtNavMeshQuery* navQuery, const glm::vec3& pos, float maxHorizontal, float maxBelow, float maxAbove)
			: m_NavQuery(navQuery), m_Pos(pos), m_MaxHorizontal2(maxHorizontal* maxHorizontal), m_MaxBelow(maxBelow), m_MaxAbove(maxAbove)
		{}

		void process(const dtMeshTile* tile, dtPoly** polys, dtPolyRef* refs, int count) override
		{
			for (int i = 0; i < count; ++i)
			{
				glm::vec3 closest;
				if (dtStatusFailed(m_NavQuery->closestPointOnPoly(refs[i], &m_Pos[0], &closest[0], nullptr)))
					continue;

				const float dx = m_Pos.x - closest.x;
				const float dz = m_Pos.z - closest.z;
				const float horizontal2 = dx * dx + dz * dz;
				const float above = m_Pos.y - closest.y; // How far `pos` is above the nav mesh point
				if (horizontal2 > m_MaxHorizontal2 || above > m_MaxAbove || above < -m_MaxBelow)
					continue;

				// Prefer the point that's horizontally nearest, then the one that's vertically nearest
				const float verticalDist = glm::abs(above);
				if (m_Found == 0 || horizontal2 < m_BestHorizontal2 || (horizontal2 == m_BestHorizontal2 && verticalDist < m_BestVertical))
				{
					m_Found = refs[i];
					m_BestPoint = closest;
					m_BestHorizontal2 = horizontal2;
					m_BestVertical = verticalDist;
				}
			}
		}

		dtPolyRef GetFound() const { return m_Found; }
		const glm::vec3& GetBestPoint() const { return m_BestPoint; }

	private:
		const dtNavMeshQuery* m_NavQuery;
		glm::vec3 m_Pos;
		float m_MaxHorizontal2;
		float m_MaxBelow;
		float m_MaxAbove;

		dtPolyRef m_Found = 0;
		glm::vec3 m_BestPoint = glm::vec3(0.f);
		float m_BestHorizontal2 = 0.f;
		float m_BestVertical = 0.f;
	};

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

	// Same as `rcCalcGridSize`, but it's computed in double precision and returns false if the grid doesn't fit in an int
	static bool CalcGridSize(const float* minBounds, const float* maxBounds, const float cellSize, int* sizeX, int* sizeZ)
	{
		const double x = (double(maxBounds[0]) - double(minBounds[0])) / double(cellSize) + 0.5;
		const double z = (double(maxBounds[2]) - double(minBounds[2])) / double(cellSize) + 0.5;
		constexpr double maxSize = double(std::numeric_limits<int>::max());
		if (!(x >= 0.0 && z >= 0.0 && x < maxSize && z < maxSize)) // Also catches NaN
		{
			*sizeX = 0;
			*sizeZ = 0;
			return false;
		}

		*sizeX = int(x);
		*sizeZ = int(z);
		return true;
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

	static void ReportError(std::string&& message)
	{
		EG_CORE_ERROR(message);
		Application::Get().GetImGuiLayer()->AddMessage(std::move(message));
	}

	Mesh::Mesh(const OverlapGeometryData& geometry, const MeshSettings& meshSettings, const AgentType& agentType, const CrowdSettings& crowdSettings)
		: m_Settings(meshSettings)
		, m_AgentType(agentType)
	{
		m_Settings.Clamp();
		m_AgentType.Clamp();

		m_Filter.setIncludeFlags(SAMPLE_POLYFLAGS_ALL ^ SAMPLE_POLYFLAGS_DISABLED);
		m_Filter.setExcludeFlags(0);
		m_Filter.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
		m_Filter.setAreaCost(SAMPLE_POLYAREA_WATER, 10.0f);
		m_Filter.setAreaCost(SAMPLE_POLYAREA_DOOR, 1.0f);
		m_Filter.setAreaCost(SAMPLE_POLYAREA_JUMP, 1.5f);

		m_Compressor = MakeScope<TileCacheCompressor>();
		m_MeshProcess = MakeScope<TileCacheMeshProcess>();

		if (!InitConfig())
			return;
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
			ReportError("[AINavigation] Failed to build chunky mesh");
			return;
		}

		const int tw = m_TilesX;
		const int th = m_TilesZ;

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

		int failedTileColumns = 0;
		for (int y = 0; y < th; ++y)
		{
			for (int x = 0; x < tw; ++x)
			{
				const dtStatus status = m_TileCache->buildNavMeshTilesAt(x, y, m_NavMesh);
				if (dtStatusFailed(status))
					++failedTileColumns;
			}
		}
		if (failedTileColumns > 0)
		{
			ReportError(std::format("[AINavigation] Failed to add the tiles of {} tile column(s) to the nav mesh, so parts of it are missing. "
				"Detour can address at most {} tile layers. Increase Tile Size or Cell Size, decrease Expected Layers per tile, or make the volume smaller",
				failedTileColumns, 1u << MeshSettings::MaxTileBits));
		}

		m_Crowd.SetSearchHalfExtents(GetSearchHalfExtents());
		m_Crowd.Init(crowdSettings, m_NavMesh, m_NavQuery, m_Filter);

		// The source geometry belongs to the caller and isn't needed after the build
		m_Vertices = nullptr;
		m_VertexCount = 0;
		m_TriangleData = nullptr;
		m_TriangleCount = 0;
		m_ChunkyMesh.Reset();
		m_ChunkyMesh.nnodes = 0;
		m_ChunkyMesh.ntris = 0;
		m_CIDs = {};
		m_TrianglesAreas = {};
		m_Tiles = {};

		m_bValid = true;
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
		if (!m_bValid)
			return;

		UpdateTileCache(ts, MaxTileRebuildsPerUpdate);
		m_Crowd.Update(ts);
	}

	bool Mesh::UpdateTileCache(float dt, uint32_t maxCalls)
	{
		bool bUpToDate = false;
		for (uint32_t i = 0; i < maxCalls && !bUpToDate; ++i)
			m_TileCache->update(dt, m_NavMesh, &bUpToDate);

		if (bUpToDate)
			m_BatchTiles.clear();

		return bUpToDate;
	}

	void Mesh::FlushObstacleRequests()
	{
		// Worst case when a full batch of tiles is waiting (that many calls), then the next call takes the queued requests
		// and rebuilds one of their (at most a full batch of) tiles, and the rest take one call each
		const uint32_t maxCalls = 2u * GetMaxTilesPerBatch();
		if (!UpdateTileCache(0.f, maxCalls))
			EG_CORE_ERROR("[AINavigation] Failed to process obstacle requests in {} tile cache updates", maxCalls);
	}

	void Mesh::ReserveBatchTiles(const dtCompressedTileRef* tiles, uint32_t count)
	{
		uint32_t newTiles = 0u;
		for (uint32_t i = 0; i < count; ++i)
		{
			if (!std::ranges::contains(m_BatchTiles, tiles[i]))
				++newTiles;
		}

		if (newTiles == 0u)
			return;

		// Detour would skip the tiles past its limit. Process the queued requests first, so that this one starts a new batch
		if (m_BatchTiles.size() + newTiles > GetMaxTilesPerBatch())
			FlushObstacleRequests();

		for (uint32_t i = 0; i < count; ++i)
		{
			if (!std::ranges::contains(m_BatchTiles, tiles[i]))
				m_BatchTiles.push_back(tiles[i]);
		}
	}

	void Mesh::ReserveBatchTiles(const float* bmin, const float* bmax)
	{
		// The same tiles Detour finds when it takes the request (`dtTileCache::update`)
		dtCompressedTileRef touched[DT_MAX_TOUCHED_TILES];
		int touchedCount = 0;
		m_TileCache->queryTiles(bmin, bmax, touched, &touchedCount, DT_MAX_TOUCHED_TILES);
		ReserveBatchTiles(touched, (uint32_t)touchedCount);
	}

	void Mesh::WarnIfObstacleTouchesTooManyTiles(const float* bmin, const float* bmax)
	{
		if (m_bWarnedLargeObstacle)
			return;

		dtCompressedTileRef touched[DT_MAX_TOUCHED_TILES + 1];
		int touchedCount = 0;
		m_TileCache->queryTiles(bmin, bmax, touched, &touchedCount, DT_MAX_TOUCHED_TILES + 1);
		if (touchedCount > DT_MAX_TOUCHED_TILES)
		{
			m_bWarnedLargeObstacle = true;

			ReportError(std::format("[AINavigation] An obstacle touches more than {} tile layers. Detour only carves the first {} of them, so part of the obstacle stays walkable. "
				"Increase the nav mesh's Tile Size or Cell Size, or use smaller obstacles. (Reported once per nav mesh)", DT_MAX_TOUCHED_TILES, DT_MAX_TOUCHED_TILES));
		}
	}

	dtObstacleRef Mesh::AddCylinderObstacle(const glm::vec3& pos, float radius, float height)
	{
		if (!m_bValid)
			return 0;

		// Same bounds as `dtTileCache::getObstacleBounds` uses for cylinders
		const float bmin[3] = { pos.x - radius, pos.y, pos.z - radius };
		const float bmax[3] = { pos.x + radius, pos.y + height, pos.z + radius };
		ReserveBatchTiles(bmin, bmax);

		dtObstacleRef obstacle = 0;
		dtStatus status = m_TileCache->addObstacle(&pos[0], radius, height, &obstacle);
		if (dtStatusFailed(status) && dtStatusDetail(status, DT_BUFFER_TOO_SMALL))
		{
			// The request queue is full
			FlushObstacleRequests();
			status = m_TileCache->addObstacle(&pos[0], radius, height, &obstacle);
		}
		if (dtStatusFailed(status))
		{
			EG_CORE_ERROR("[AINavigation] Failed to add a cylinder obstacle. Error code: {}", status);
			return 0;
		}

		WarnIfObstacleTouchesTooManyTiles(bmin, bmax);

		return obstacle;
	}

	dtObstacleRef Mesh::AddBoxObstacle(const glm::vec3& pos, const glm::vec3& halfExtents, float yRadians)
	{
		if (!m_bValid)
			return 0;

		// Same bounds as `dtTileCache::getObstacleBounds` uses for oriented boxes
		const float maxr = 1.41f * glm::max(halfExtents.x, halfExtents.z);
		const float bmin[3] = { pos.x - maxr, pos.y - halfExtents.y, pos.z - maxr };
		const float bmax[3] = { pos.x + maxr, pos.y + halfExtents.y, pos.z + maxr };
		ReserveBatchTiles(bmin, bmax);

		dtObstacleRef obstacle = 0;
		dtStatus status = m_TileCache->addBoxObstacle(&pos[0], &halfExtents[0], yRadians, &obstacle);
		if (dtStatusFailed(status) && dtStatusDetail(status, DT_BUFFER_TOO_SMALL))
		{
			// The request queue is full
			FlushObstacleRequests();
			status = m_TileCache->addBoxObstacle(&pos[0], &halfExtents[0], yRadians, &obstacle);
		}
		if (dtStatusFailed(status))
		{
			EG_CORE_ERROR("[AINavigation] Failed to add a box obstacle. Error code: {}", status);
			return 0;
		}

		WarnIfObstacleTouchesTooManyTiles(bmin, bmax);

		return obstacle;
	}

	bool Mesh::RemoveObstacle(dtObstacleRef obstacle)
	{
		if (!m_bValid)
			return false;

		// A removal rebuilds the tiles the obstacle touched when it was added
		if (const dtTileCacheObstacle* ob = m_TileCache->getObstacleByRef(obstacle))
			ReserveBatchTiles(ob->touched, ob->ntouched);

		dtStatus status = m_TileCache->removeObstacle(obstacle);
		if (dtStatusFailed(status) && dtStatusDetail(status, DT_BUFFER_TOO_SMALL))
		{
			// The request queue is full
			FlushObstacleRequests();
			status = m_TileCache->removeObstacle(obstacle);
		}
		const bool bSuccess = dtStatusSucceed(status);
		if (!bSuccess)
		{
			EG_CORE_ERROR("[AINavigation] Failed to remove an obstacle. Error code: {}", status);
		}
		return bSuccess;
	}

	void Mesh::SetSearchHalfExtents(const std::optional<glm::vec3>& searchHalfExtents)
	{
		m_Settings.OverrideSearchExtents = searchHalfExtents.has_value();
		if (searchHalfExtents)
			m_Settings.SearchHalfExtents = glm::max(*searchHalfExtents, glm::vec3(MeshSettings::MinSearchExtent));

		m_Crowd.SetSearchHalfExtents(GetSearchHalfExtents());
	}

	dtPolyRef Mesh::FindNearestPoly(const glm::vec3& pos, const std::optional<glm::vec3>& searchHalfExtents, glm::vec3* outNearestPoint) const
	{
		const glm::vec3 extents = searchHalfExtents ? *searchHalfExtents : GetSearchHalfExtents();
		if (extents.x <= 0.f || extents.y <= 0.f || extents.z <= 0.f)
			return 0;

		dtPolyRef poly = 0;
		glm::vec3 nearest = pos;
		if (dtStatusFailed(m_NavQuery->findNearestPoly(&pos[0], &extents[0], &m_Filter, &poly, &nearest[0])))
			return 0;

		if (outNearestPoint && poly != 0)
			*outNearestPoint = nearest;

		return poly;
	}

	std::vector<glm::vec3> Mesh::FindStraightPath(const glm::vec3& start, const glm::vec3& end, uint32_t maxPolys, const std::optional<glm::vec3>& searchHalfExtents) const
	{
		if (!m_bValid || maxPolys == 0u)
			return {};

		const dtPolyRef startPoly = FindNearestPoly(start, searchHalfExtents);
		const dtPolyRef endPoly = FindNearestPoly(end, searchHalfExtents);
		if (startPoly == 0 || endPoly == 0)
			return {};

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
		int nstraightPath = 0;
		int straightPathOptions = 0;

		const dtStatus status = m_NavQuery->findStraightPath(&start[0], epos, polys.data(), npolys,
			&straightPath[0].x, straightPathFlags.data(),
			straightPathPolys.data(), &nstraightPath, maxPolys, straightPathOptions);
		if (dtStatusFailed(status))
			return {};

		straightPath.resize(glm::clamp(nstraightPath, 0, (int)maxPolys));
		return straightPath;
	}

	std::vector<glm::vec3> Mesh::FindSmoothPath(const glm::vec3& start, const glm::vec3& end, uint32_t maxPolys, uint32_t maxSmooth, const std::optional<glm::vec3>& searchHalfExtents) const
	{
		if (!m_bValid || maxPolys == 0u || maxSmooth == 0u)
			return {};

		const dtPolyRef startPoly = FindNearestPoly(start, searchHalfExtents);
		const dtPolyRef endPoly = FindNearestPoly(end, searchHalfExtents);
		if (startPoly == 0 || endPoly == 0)
			return {};

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
		if (dtStatusFailed(m_NavQuery->closestPointOnPoly(startPoly, &start[0], iterPos, 0)) ||
			dtStatusFailed(m_NavQuery->closestPointOnPoly(polys[npolys - 1], &end[0], targetPos, 0)))
		{
			return {};
		}

		static const float STEP_SIZE = 0.5f;
		static const float SLOP = 0.01f;

		nsmoothPath = 0;

		dtVcopy(&smoothPath[nsmoothPath].x, iterPos);
		nsmoothPath++;

		// Move towards target a small advancement at a time until target reached or
		// when ran out of memory to store the path.
		while (npolys && nsmoothPath < (int)maxSmooth)
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

			// Keep the height from `moveAlongSurface` if Detour can't find one (it fails for points on a polygon's edge)
			float h = 0;
			if (dtStatusSucceed(m_NavQuery->getPolyHeight(polys[0], result, &h)))
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
						if ((nsmoothPath & 1) && nsmoothPath < (int)maxSmooth)
						{
							dtVcopy(&smoothPath[nsmoothPath].x, startPos);
							nsmoothPath++;
						}
					}
					// Move position at the other side of the off-mesh link.
					dtVcopy(iterPos, endPos);
					float eh = 0.0f;
					if (dtStatusSucceed(m_NavQuery->getPolyHeight(polys[0], iterPos, &eh)))
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

	bool Mesh::FindDistanceToWall(const glm::vec3& pos, float maxRadius, glm::vec3* outHitPos, glm::vec3* outHitNormal, float* outHitDistance, const std::optional<glm::vec3>& searchHalfExtents) const
	{
		if (!m_bValid)
			return false;

		glm::vec3 center = pos;
		const dtPolyRef poly = FindNearestPoly(pos, searchHalfExtents, &center);
		if (poly == 0)
			return false;

		const dtStatus status = m_NavQuery->findDistanceToWall(poly, &center[0], maxRadius, &m_Filter, outHitDistance, &outHitPos->x, &outHitNormal->x);
		return dtStatusSucceed(status);
	}

	bool Mesh::FindRandomPoint(glm::vec3* outRandomPoint) const
	{
		if (!m_bValid)
			return false;

		dtPolyRef randomPoly = 0;
		const dtStatus status = m_NavQuery->findRandomPoint(&m_Filter, Random::Float, &randomPoly, &outRandomPoint->x);
		return dtStatusSucceed(status);
	}

	bool Mesh::FindRandomPointInCircle(const glm::vec3& pos, float radius, glm::vec3* outRandomPoint, const std::optional<glm::vec3>& searchHalfExtents) const
	{
		if (!m_bValid)
			return false;

		glm::vec3 center = pos;
		const dtPolyRef poly = FindNearestPoly(pos, searchHalfExtents, &center);
		if (poly == 0)
			return false;

		dtPolyRef randomPoly = 0;
		const dtStatus status = m_NavQuery->findRandomPointAroundCircle(poly, &center.x, radius, &m_Filter, Random::Float, &randomPoly, &outRandomPoint->x);
		return dtStatusSucceed(status);
	}

	bool Mesh::FindNearestPoint(const glm::vec3& pos, glm::vec3* outNearestPoint, const std::optional<glm::vec3>& searchHalfExtents) const
	{
		if (!m_bValid)
			return false;

		glm::vec3 nearest = pos;
		if (FindNearestPoly(pos, searchHalfExtents, &nearest) == 0)
			return false;

		if (outNearestPoint)
			*outNearestPoint = nearest;

		return true;
	}

	bool Mesh::IsValidPoint(const glm::vec3& pos, glm::vec3* outNavMeshPoint) const
	{
		if (!m_bValid)
			return false;

		const float maxHorizontal = m_Settings.GetAgentReach(m_AgentType);
		const float maxAbove = m_AgentType.Height;
		const float maxBelow = m_AgentType.MaxClimb; // Nav mesh heights are approximate, so the real ground can be a bit below it

		// The box that contains every nav mesh point that can be within the agent's space.
		// `maxAbove`: pos may be up to this far above the nav mesh point. Seen from pos, the nav mesh point may be up to `maxAbove` below it.
		// `maxBelow`: pos may be up to this far below the nav mesh point. Seen from pos, the nav mesh point may be up to `maxBelow` above it.
		// So, the nav mesh points the box must contain have heights in [pos.y - maxAbove, pos.y + maxBelow] range.
		const glm::vec3 center = glm::vec3(pos.x, pos.y + (maxBelow - maxAbove) * 0.5f, pos.z);
		
		const glm::vec3 halfExtents = glm::vec3(maxHorizontal, (maxAbove + maxBelow) * 0.5f, maxHorizontal);
		AgentSpaceQuery query(m_NavQuery, pos, maxHorizontal, maxBelow, maxAbove);
		if (dtStatusFailed(m_NavQuery->queryPolygons(&center[0], &halfExtents[0], &m_Filter, &query)))
			return false;

		if (query.GetFound() == 0)
			return false;

		if (outNavMeshPoint)
			*outNavMeshPoint = query.GetBestPoint();

		return true;
	}

	bool Mesh::HasCompletePath(const glm::vec3& from, const glm::vec3& to, const std::optional<glm::vec3>& searchHalfExtents) const
	{
		if (!m_bValid)
			return false;

		glm::vec3 startPos = from;
		glm::vec3 endPos = to;

		const dtPolyRef startPoly = FindNearestPoly(from, searchHalfExtents, &startPos);
		const dtPolyRef endPoly = FindNearestPoly(to, searchHalfExtents, &endPos);
		if (startPoly == 0 || endPoly == 0)
			return false;

		dtPolyRef path[16];
		int pathCount = 0;
		const dtStatus status = m_NavQuery->findPath(startPoly, endPoly, &startPos[0], &endPos[0], &m_Filter, path, &pathCount, (int)std::size(path));
		return dtStatusSucceed(status) && !dtStatusDetail(status, DT_PARTIAL_RESULT);
	}

	void Mesh::GetDebugDraw(duDebugDraw* debugDraw) const
	{
		if (!m_bValid)
			return;

		// Polygons are colored by area (see `duDebugDraw::areaToCol`), so that the debug draw can color each nav mesh by its agent type
		constexpr uint8_t drawFlags = DU_DRAWNAVMESH_OFFMESHCONS | DU_DRAWNAVMESH_CLOSEDLIST;

		//drawTiles(debugDraw, m_TileCache);
		//drawObstacles(debugDraw, m_TileCache);
		duDebugDrawNavMeshWithClosedList(debugDraw, *m_NavMesh, *m_NavQuery, drawFlags);
	}

	Ref<Mesh> Mesh::Create(const OverlapGeometryData& geometry, const MeshSettings& settings, const AgentType& agentType, const CrowdSettings& crowdSettings)
	{
		Ref<Mesh> mesh = MakeRef<Mesh>(geometry, settings, agentType, crowdSettings);
		if (!mesh->m_bValid)
		{
			EG_CORE_ERROR("[AINavigation] Failed to build the navigation mesh");
			return nullptr;
		}
		return mesh;
	}

	bool Mesh::InitConfig()
	{
		const float* bmin = glm::value_ptr(m_Settings.Bounds.Min);
		const float* bmax = glm::value_ptr(m_Settings.Bounds.Max);

		{
			constexpr float DetailSampleDist = 6.f;
			constexpr float DetailSampleMaxError = 1.f;

			rcConfig& cfg = m_Config;
			memset(&cfg, 0, sizeof(cfg));
			cfg.cs = m_Settings.CellSize;
			cfg.ch = m_Settings.CellHeight;
			cfg.walkableSlopeAngle = m_AgentType.MaxSlope;
			cfg.walkableHeight = (int)ceilf(m_AgentType.Height / cfg.ch);
			cfg.walkableClimb = (int)floorf(m_AgentType.MaxClimb / cfg.ch);
			cfg.walkableRadius = (int)ceilf(m_AgentType.Radius / cfg.cs);
			cfg.maxSimplificationError = m_Settings.EdgeMaxError;
			cfg.tileSize = (int)m_Settings.TileSize;
			cfg.borderSize = cfg.walkableRadius + (int)m_Settings.BorderSize; // Reserve enough padding.
			cfg.detailSampleDist = DetailSampleDist < 0.9f ? 0 : m_Settings.CellSize * DetailSampleDist;
			cfg.detailSampleMaxError = m_Settings.CellHeight * DetailSampleMaxError;
			rcVcopy(cfg.bmin, bmin);
			rcVcopy(cfg.bmax, bmax);
			if (!CalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height))
			{
				ReportError(std::format("[AINavigation] The nav mesh volume is too large for its Cell Size ({})", cfg.cs));
				return false;
			}
			if (cfg.width == 0 || cfg.height == 0)
			{
				ReportError(std::format("[AINavigation] The nav mesh volume is empty. Its size on X and Z must be at least the Cell Size ({})", cfg.cs));
				return false;
			}
		}

		// Tile grid. Computed in 64 bits, since `tiles * layers` can overflow an int
		{
			const int64_t ts = (int64_t)m_Settings.TileSize;
			const int64_t tw = ((int64_t)m_Config.width + ts - 1) / ts;
			const int64_t th = ((int64_t)m_Config.height + ts - 1) / ts;
			const uint64_t tileLayers = uint64_t(tw) * uint64_t(th) * uint64_t(m_Settings.ExpectedLayersPerTile);
			if (tileLayers > (uint64_t)std::numeric_limits<int>::max())
			{
				ReportError(std::format("[AINavigation] The nav mesh volume is too large: {}x{} tiles with {} expected layers per tile. "
					"Increase Tile Size or Cell Size, or make the volume smaller", tw, th, m_Settings.ExpectedLayersPerTile));
				return false;
			}
			// Note: more than 2^MaxTileBits tile layers can't be addressed by the nav mesh. It's not reported here, since
			// `ExpectedLayersPerTile` is only an estimate. Tiles that really don't fit are reported after the build

			m_TilesX = (int)tw;
			m_TilesZ = (int)th;
		}

		// Tile cache params.
		{
			const int tw = m_TilesX;
			const int th = m_TilesZ;

			dtTileCacheParams& tcparams = m_TileCacheConfig;
			memset(&tcparams, 0, sizeof(tcparams));
			rcVcopy(tcparams.orig, bmin);
			tcparams.cs = m_Settings.CellSize;
			tcparams.ch = m_Settings.CellHeight;
			tcparams.width = (int)m_Settings.TileSize;
			tcparams.height = (int)m_Settings.TileSize;
			tcparams.walkableHeight = m_AgentType.Height;
			tcparams.walkableRadius = m_AgentType.Radius;
			tcparams.walkableClimb = m_AgentType.MaxClimb;
			tcparams.maxSimplificationError = m_Settings.EdgeMaxError;
			tcparams.maxTiles = (int)(tw * th * (int)m_Settings.ExpectedLayersPerTile); // Checked above to fit in an int
			tcparams.maxObstacles = (int)m_Settings.MaxObstacles;
		}

		return true;
	}

	bool Mesh::CreateTileCache()
	{
		dtFreeTileCache(m_TileCache);
		m_TileCache = dtAllocTileCache();
		if (!m_TileCache)
		{
			ReportError("[AINavigation] Failed to allocate TileCache");
			return false;
		}

		auto status = m_TileCache->init(&m_TileCacheConfig, &m_Allocator, m_Compressor.get(), m_MeshProcess.get());
		if (dtStatusFailed(status))
		{
			ReportError("[AINavigation] Failed to init TileCache");
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
			ReportError("[AINavigation] Failed to allocate NavMesh");
			return false;
		}

		const float* bmin = m_Config.bmin;

		// Max tiles and max polys affect how the tile IDs are caculated.
		// There are 22 bits available for identifying a tile and a polygon.
		// Note: `InitConfig` made sure that it fits in an int
		const uint32_t tileLayers = uint32_t(m_TilesX) * uint32_t(m_TilesZ) * m_Settings.ExpectedLayersPerTile;
		const int tileBits = rcMin((int)log2(nextPow2(tileLayers)), (int)MeshSettings::MaxTileBits);
		const int polyBits = 22 - tileBits;

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
			ReportError("[AINavigation] Failed to init NavMesh");
			return false;
		}

		dtFreeNavMeshQuery(m_NavQuery);
		m_NavQuery = dtAllocNavMeshQuery();
		if (!m_NavQuery)
		{
			ReportError("[AINavigation] Failed to allocate NavMesh query");
			return false;
		}

		status = m_NavQuery->init(m_NavMesh, m_Settings.MaxQueryNodes);
		if (dtStatusFailed(status))
		{
			ReportError("[AINavigation] Failed to init NavMesh query");
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
		CalcGridSize(tcfg.bmin, tcfg.bmax, tcfg.cs, &tcfg.width, &tcfg.height);
		
		// Allocate voxel height field where we rasterize our input data to.
		rcFreeHeightField(m_Solid);
		m_Solid = rcAllocHeightfield();
		if (!m_Solid)
		{
			ReportError("[AINavigation] Failed to allocate Heightfield");
			return false;
		}
		if (!rcCreateHeightfield(&m_Context, *m_Solid, tcfg.width, tcfg.height,
			tcfg.bmin, tcfg.bmax, tcfg.cs, tcfg.ch))
		{
			ReportError("[AINavigation] Failed to create Heightfield");
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
				ReportError(std::format("[AINavigation] Failed to rasterize triangles at tile {}x{}, ID: {}", x, y, i));
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
			ReportError("[AINavigation] Failed to allocate CompactHeightfield");
			return false;
		}
		if (!rcBuildCompactHeightfield(&m_Context, m_Config.walkableHeight, m_Config.walkableClimb, *m_Solid, *m_CompactHeightfield))
		{
			ReportError("[AINavigation] Failed to build CompactHeightfield");
			return false;
		}

		rcFreeHeightField(m_Solid);
		m_Solid = nullptr;

		// Erode the walkable area by agent radius.
		if (!rcErodeWalkableArea(&m_Context, m_Config.walkableRadius, *m_CompactHeightfield))
		{
			ReportError("[AINavigation] Failed to erode Walkable Area");
			return false;
		}

		rcFreeHeightfieldLayerSet(m_HeightfieldLayerSet);
		m_HeightfieldLayerSet = rcAllocHeightfieldLayerSet();
		if (!m_HeightfieldLayerSet)
		{
			ReportError("[AINavigation] Failed to allocate Heightfield Layer Set");
			return false;
		}
		if (!rcBuildHeightfieldLayers(&m_Context, *m_CompactHeightfield, m_Config.borderSize, m_Config.walkableHeight, *m_HeightfieldLayerSet))
		{
			ReportError("[AINavigation] Failed to build Heightfield Layers");
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
				ReportError("[AINavigation] Failed to build TileCache layer");
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
				ReportError("[AINavigation] Failed to add tile");
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
			ReportError("[AINavigation] Failed to add tile");
			dtFree(tileData.m_data);
			return false;
		}

		return true;
	}
}
