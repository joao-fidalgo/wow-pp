//
// This file is part of the WoW++ project.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
//

#include "pch.h"
#include "map.h"
#include "common/macros.h"
#include "common/linear_set.h"
#include "game/constants.h"
#include "circle.h"
#include "shared/proto_data/maps.pb.h"
#include "log/default_log_levels.h"
#include "common/make_unique.h"
#include "math/vector3.h"
#include "math/aabb_tree.h"
#include "common/clock.h"
#include "detour/DetourCommon.h"
#include "recast/Recast.h"
#include "binary_io/stream_source.h"
#include "binary_io/reader.h"
#include "cppformat/cppformat/format.h"

namespace wowpp
{
	std::map<UInt32, std::unique_ptr<dtNavMesh, NavMeshDeleter>> Map::navMeshsPerMap;
	std::map<String, std::shared_ptr<math::AABBTree>> aabbTreeById;
	std::map<String, std::shared_ptr<math::AABBTree>> aabbDoodadTreeById;

	Map::Map(const proto::MapEntry &entry, boost::filesystem::path dataPath, bool loadDoodads/* = false*/)
		: m_entry(entry)
		, m_dataPath(std::move(dataPath))
		, m_tiles(64, 64)
		, m_navMesh(nullptr)
		, m_navQuery(nullptr)
		, m_loadDoodads(loadDoodads)
	{
		setupNavMesh();
	}

	void Map::setupNavMesh()
	{
		// Allocate navigation mesh
		auto it = navMeshsPerMap.find(m_entry.id());
		if (it == navMeshsPerMap.end())
		{
			// Build file name
			std::ostringstream strm;
			strm << (m_dataPath / "maps").string() << "/" << m_entry.id() << ".map";

			const String file = strm.str();
			if (!boost::filesystem::exists(file))
			{
				// File does not exist
				DLOG("Could not load map file " << file << ": File does not exist");
				return;
			}

			// Open file for reading
			std::ifstream mapFile(file.c_str(), std::ios::in | std::ios::binary);
			if (!mapFile)
			{
				ELOG("Could not load map file " << file);
				return;
			}

			dtNavMeshParams params;
			memset(&params, 0, sizeof(dtNavMeshParams));
			if (!(mapFile.read(reinterpret_cast<char*>(&params), sizeof(dtNavMeshParams))))
			{
				ELOG("Map file " << file << " seems to be corrupted!");
				return;
			}

			auto navMesh = std::unique_ptr<dtNavMesh, NavMeshDeleter>(dtAllocNavMesh());
			if (!navMesh)
			{
				ELOG("Could not allocate navigation mesh!");
				return;
			}

			dtStatus result = navMesh->init(&params);
			if (!dtStatusSucceed(result))
			{
				ELOG("Could not initialize navigation mesh: " << result);
				return;
			}

			// At this point, it's just an empty mesh without tiles. Tiles will be loaded
			// when required.
			m_navMesh = navMesh.get();

			// allocate mesh query
			m_navQuery.reset(dtAllocNavMeshQuery());
			ASSERT(m_navQuery);

			dtStatus dtResult = m_navQuery->init(m_navMesh, 1024);
			if (dtStatusFailed(dtResult))
			{
				m_navQuery.reset();
			}

			// Setup filter
			m_filter.setIncludeFlags(1 | 2 | 4 | 8 | 16 | 32);		// Testing...
			m_adtSlopeFilter.setIncludeFlags(1 | 2 | 4 | 8 | 16);
			m_adtSlopeFilter.setExcludeFlags(32);

			navMeshsPerMap[m_entry.id()] = std::move(navMesh);
		}
	}

	void Map::loadAllTiles()
	{
		// Load all tiles
		for (size_t x = 0; x < 64; ++x)
		{
			for (size_t y = 0; y < 64; ++y)
			{
				getTile(TileIndex2D(x, y));
			}
		}
	}

	void Map::unloadAllTiles()
	{
		// Remove all loaded tile data
		m_tiles.clear();

		// Destroy nav mesh
		auto it = navMeshsPerMap.find(m_entry.id());
		if (it != navMeshsPerMap.end())
		{
			it = navMeshsPerMap.erase(it);

			// Reconstruct a new empty nav mesh
			setupNavMesh();
		}
	}

	MapDataTile *Map::getTile(const TileIndex2D &position)
	{
		if ((position[0] >= 0) &&
			(position[0] < static_cast<TileIndex>(m_tiles.width())) &&
			(position[1] >= 0) &&
			(position[1] < static_cast<TileIndex>(m_tiles.height())))
		{
			auto tile = m_tiles(position[0], position[1]);
			if (!tile)
			{
				tile = loadTile(position);
			}

			return tile.get();
		}

		return nullptr;
	}

	bool Map::getHeightAt(const math::Vector3 &pos, float &out_height, bool sampleADT, bool sampleWMO)
	{
		TileIndex2D tileIndex(
			static_cast<Int32>(floor((32.0 - (static_cast<double>(pos.x) / 533.3333333)))),
			static_cast<Int32>(floor((32.0 - (static_cast<double>(pos.y) / 533.3333333))))
		);

		bool hit = false;
		float hitHeight = 0.0f;
		auto *tile = getTile(tileIndex);
		if (tile)
		{
			if (sampleWMO)
			{
				auto rayStart = (pos + math::Vector3(0.0f, 0.0f, 0.5f));
				auto rayEnd = (pos + math::Vector3(0.0f, 0.0f, -7.0f));
				math::Ray ray(rayStart, rayEnd);

				for (const auto &wmo : tile->wmos.entries)
				{
					if (ray.intersectsAABB(wmo.bounds).first)
					{
						// WMO was hit, now we transform the ray into WMO coordinate space and do the check again
						math::Ray transformedRay(
							wmo.inverse * rayStart,
							wmo.inverse * rayEnd
						);

						auto treeIt = aabbTreeById.find(wmo.fileName);
						if (treeIt != aabbTreeById.end())
						{
							if (treeIt->second->intersectRay(transformedRay, nullptr, math::raycast_flags::IgnoreBackface))
							{
								hit = true;
								hitHeight = rayStart.lerp(rayEnd, transformedRay.hitDistance).z;
								break;
							}
						}
					}
				}
			}
			
		}

		if (hit)
		{
			out_height = hitHeight;
		}

		return hit;
	}

	bool Map::isInLineOfSight(const math::Vector3 &posA, const math::Vector3 &posB)
	{
		if (posA == posB)
			return true;

		// Skip checks if too much distance
		const float maxCheckDistSq = constants::MapWidth * constants::MapWidth;
		if ((posA - posB).squared_length() >= maxCheckDistSq)
			return true;

		// Create a ray
		math::Ray ray(posA, posB);

		// False if blocked
		bool inLineOfSight = true;
		
		// Keep track of checked WMO ids - this is required since multiple tiles might reference the same
		// WMOs and we don't want to check WMO's twice for the same ray cast (especially large cities!)
		LinearSet<UInt32> checkedWmos;

		// Now process every tile on the way
		forEachTileInRayXY(ray, constants::MapWidth, [&](Int32 x, Int32 y) -> bool {
			auto tileIndex = TileIndex2D(static_cast<Int32>(floor(31.0f - x)), static_cast<Int32>(floor(31.0f - y)));
			auto *tile = getTile(tileIndex);
			if (!tile)
			{
				// Failed to obtain tile, skip
				WLOG("Failed to obtain tile " << tileIndex);
				return true;
			}

			// Now check each wmo
			for (const auto &wmo : tile->wmos.entries)
			{
				// Check if we already checked this wmo once
				if (checkedWmos.contains(wmo.uniqueId))
					continue;

				// Do a bounding box check to see if this wmo is hit by the ray cast
				auto result = ray.intersectsAABB(wmo.bounds);
				if (result.first)
				{
					// WMO was hit - keep track of it
					checkedWmos.add(wmo.uniqueId);

					// WMO was hit, now we transform the ray into WMO coordinate space and do the check again
					math::Ray transformedRay(
						wmo.inverse * posA,
						wmo.inverse * posB
					);

					// Find AABBTree instance and execute raycast
					auto treeIt = aabbTreeById.find(wmo.fileName);
					if (treeIt != aabbTreeById.end())
					{
						if (treeIt->second->intersectRay(transformedRay, nullptr, math::raycast_flags::EarlyExit))
						{
							// We hit something, so stop iterating here
							inLineOfSight = false;
							return false;
						}
					}
				}
			}

			// Process next tile
			return true;
		});

		return inLineOfSight;
	}
	
#define MAX_PATH_LENGTH         74
#define MAX_POINT_PATH_LENGTH   74
#define SMOOTH_PATH_STEP_SIZE   4.0f
#define SMOOTH_PATH_SLOP        0.08f
#define VERTEX_SIZE       3
#define INVALID_POLYREF   0

	dtStatus smoothPath(dtNavMeshQuery &query, dtNavMesh &navMesh, dtQueryFilter &filter, std::vector<dtPolyRef> &polyPath, std::vector<math::Vector3> &waypoints)
	{
		static constexpr float PointDist = 4.0f;	// One point every PointDist units
		static constexpr float Extents[3] = { 1.0f, 50.0f, 1.0f };

		// Travel along the path and insert new points in between, start with the second point
		size_t polyIndex = 0;
		for (size_t p = 1; p < waypoints.size(); ++p)
		{
			// Get the previous point
			const auto prevPoint = waypoints[p - 1];
			const auto thisPoint = waypoints[p];

			// Get the direction vector and the distance
			auto dir = thisPoint - prevPoint;
			auto dist = dir.normalize();
			const Int32 count = dist / PointDist;
			const float step = dist / count;

			// Now insert new points
			for (Int32 n = 1; n < count; n++)
			{
				const float d = n * step;
				auto newPoint = prevPoint + (dir * d);

				math::Vector3 closestPoint;
				dtPolyRef nearestPoly = 0;
				if (dtStatusFailed(query.findNearestPoly(&newPoint.x, Extents, &filter, &nearestPoly, &closestPoint.x)))
				{
					//WLOG("Could not find nearest poly");
				}

				// Now do the height correction
				if (dtStatusSucceed(query.getPolyHeight(nearestPoly, &closestPoint.x, &newPoint.y)))
				{
					//newPoint.y += 0.375f;
				}

				waypoints.insert(waypoints.begin() + p, newPoint);

				// Next point
				p++;
			}

			polyIndex++;
		}

		return DT_SUCCESS;
	}

	bool Map::calculatePath(const math::Vector3 & source, math::Vector3 dest, std::vector<math::Vector3>& out_path, bool ignoreAdtSlope/* = true*/, const IShape *clipping/* = nullptr*/)
	{
		// Convert the given start and end point into recast coordinate system
		math::Vector3 dtStart = wowToRecastCoord(source);
		math::Vector3 dtEnd = wowToRecastCoord(dest);

		// No nav mesh loaded for this map?
		if (!m_navMesh)
		{
			ELOG("Could not find nav mesh!");
			return false;
		}

		// TODO: Better solution for this, as there could be more tiles between which could eventually
		// still be unloaded after this block
		{
			// Load source tile
			TileIndex2D startIndex(
				static_cast<Int32>(floor((32.0 - (static_cast<double>(source.x) / 533.3333333)))),
				static_cast<Int32>(floor((32.0 - (static_cast<double>(source.y) / 533.3333333))))
			);
			if (!getTile(startIndex))
			{
				//ELOG("Could not get source tile!");
				return false;
			}

			// Load dest tile
			TileIndex2D destIndex(
				static_cast<Int32>(floor((32.0 - (static_cast<double>(dest.x) / 533.3333333)))),
				static_cast<Int32>(floor((32.0 - (static_cast<double>(dest.y) / 533.3333333))))
			);
			if (destIndex != startIndex)
			{
				if (!getTile(destIndex))
				{
					//ELOG("Could not get dest tile!");
					return false;
				}
			}
		}
		
		// Make sure that source cell is loaded
		int tx, ty;
		m_navMesh->calcTileLoc(&dtStart.x, &tx, &ty);
		if (!m_navMesh->getTileAt(tx, ty, 0))
		{
			// Not loaded (TODO: Handle this error?)
			//ELOG("Could not get source tile on nav mesh");
			return false;
		}

		// Make sure that target cell is loaded
		m_navMesh->calcTileLoc(&dtEnd.x, &tx, &ty);
		if (!m_navMesh->getTileAt(tx, ty, 0))
		{
			// Not loaded (TODO: Handle this error?)
			//ELOG("Could not get dest tile on nav mesh");
			return false;
		}

		// Find polygon on start and end point
		float distToStartPoly, distToEndPoly;
		dtPolyRef startPoly = getPolyByLocation(dtStart, distToStartPoly);
		dtPolyRef endPoly = getPolyByLocation(dtEnd, distToEndPoly);
		if (startPoly == 0 || endPoly == 0)
		{
			// Either start or target does not have a valid polygon, so we can't walk
			// from the start point or to the target point at all! (TODO: Handle this error?)
			//ELOG("Could not get source poly or dest poly");
			return false;
		}

		// We check if the distance to the start or end polygon is too far and eventually correct the target
		// location
		const bool isFarFromPoly = distToStartPoly > 7.0f || distToEndPoly > 7.0f;
		if (isFarFromPoly)
		{
			math::Vector3 closestPoint;
			if (dtStatusSucceed(m_navQuery->closestPointOnPoly(endPoly, &dtEnd.x, &closestPoint.x, nullptr)))
			{
				dtEnd = closestPoint;
				dest = recastToWoWCoord(dtEnd);
			}
		}

		// Set to true to generate straight path
		dtStatus dtResult;

		// Buffer to store polygons that need to be used for our path
		const int maxPathLength = 74;
		std::vector<dtPolyRef> tempPath(maxPathLength, 0);

		// This will store the resulting path length (number of polygons)
		int pathLength = 0;

		if (startPoly != endPoly)
		{
			dtResult = m_navQuery->findPath(
				startPoly,											// start polygon
				endPoly,											// end polygon
				&dtStart.x,											// start position
				&dtEnd.x,											// end position
				ignoreAdtSlope ? &m_filter : &m_adtSlopeFilter,		// polygon search filter
				tempPath.data(),									// [out] path
				&pathLength,										// number of polygons used by path (<= maxPathLength)
				maxPathLength);										// max number of polygons in output path
			if (!pathLength ||
				dtStatusFailed(dtResult))
			{
				// Could not find path... TODO?
				ELOG("findPath failed with result " << dtResult);
				return false;
			}
		}
		else
		{
			// Build shortcut
			tempPath[0] = startPoly;
			tempPath[1] = endPoly;
			pathLength = 2;
		}
		
		// Resize path
		tempPath.resize(pathLength);

		// Buffer to store path coordinates
		std::vector<math::Vector3> tempPathCoords(maxPathLength);
		std::vector<dtPolyRef> tempPathPolys(maxPathLength);
		std::vector<unsigned char> tempPathFlags(maxPathLength);
		int tempPathCoordsCount = 0;

		bool targetIsADT = false;
		if (startPoly != endPoly)
		{
			// Find a straight path
			dtResult = m_navQuery->findStraightPath(
				&dtStart.x,							// Start position
				&dtEnd.x,							// End position
				tempPath.data(),					// Polygon path
				static_cast<int>(tempPath.size()),	// Number of polygons in path
				&tempPathCoords[0].x,				// [out] Path points
				&tempPathFlags[0],					// [out] Path point flags
				&tempPathPolys[0],					// [out] Polygon id for each point.
				&tempPathCoordsCount,				// [out] used coordinate count in vertices (3 floats = 1 vert)
				maxPathLength,						// max coordinate count
				0									// options
			);
			if (dtStatusFailed(dtResult))
			{
				ELOG("findStraightPath failed");
				return false;
			}

			unsigned short polyFlags = 0;
			if (tempPathCoordsCount > 0)
			{
				dtPolyRef poly = tempPathPolys[tempPathCoordsCount - 1];
				if (dtStatusSucceed(m_navMesh->getPolyFlags(poly, &polyFlags)))
				{
					targetIsADT =
						(polyFlags & (2 | 32)) != 0;
				}
			}
		}
		else
		{

			// Adjust end height to the poly height here
			float newHeight = dtEnd.y;
			if (dtStatusSucceed(m_navQuery->getPolyHeight(endPoly, &dtEnd.x, &newHeight)))
				dtEnd.y = newHeight;

			unsigned short polyFlags = 0;
			if (dtStatusSucceed(m_navMesh->getPolyFlags(endPoly, &polyFlags)))
			{
				targetIsADT =
					(polyFlags & (2 | 32)) != 0;
			}

			// Build shortcut
			tempPathCoords[0] = dtStart;
			tempPathCoords[1] = dtEnd;
			tempPathPolys[0] = startPoly;
			tempPathPolys[1] = endPoly;
			tempPathCoordsCount = 2;
		}

		if (tempPathCoordsCount == 0)
			return false;

		// Correct actual path length
		tempPathCoords.resize(tempPathCoordsCount);
		tempPathPolys.resize(tempPathCoordsCount);

		// Adjust height value
		float newHeight = 0.0f;
		auto wowCoord = recastToWoWCoord(tempPathCoords.back());
		bool adjustHeight = getHeightAt(wowCoord, newHeight, targetIsADT, !targetIsADT);
		if (adjustHeight)
		{
			tempPathCoords.back().y = newHeight;
		}

		// Smooth out the path
		dtResult = smoothPath(*m_navQuery, *m_navMesh, ignoreAdtSlope ? m_filter : m_adtSlopeFilter, tempPathPolys, tempPathCoords);
		if (dtStatusFailed(dtResult))
		{
			ELOG("Failed to smooth out existing path.");
			return false;
		}

		// Append waypoints and eventually do shape clipping
		bool wasInShape = false;
		for (const auto &p : tempPathCoords)
		{
			auto wowCoord = recastToWoWCoord(p);
			if (clipping)
			{
				game::Point pos(wowCoord.x, wowCoord.y);
				if (!clipping->isPointInside(pos))
				{
					// Stop: We were in the shape already and wanted to exit it (eventually again)
					if (wasInShape)
						break;
				}
				else
				{
					// Entered the shape
					wasInShape = true;
				}
			}

			out_path.push_back(wowCoord);
		}

		return true;
	}

	dtPolyRef Map::getPolyByLocation(const math::Vector3 &point, float &out_distance) const
	{
		// TODO: Use cached poly
		dtPolyRef polyRef = 0;

		// we don't have it in our old path
		// try to get it by findNearestPoly()
		// first try with low search box
		math::Vector3 extents(3.0f, 5.0f, 3.0f);    // bounds of poly search area
		math::Vector3 closestPoint(0.0f, 0.0f, 0.0f);
		dtStatus dtResult = m_navQuery->findNearestPoly(&point.x, &extents.x, &m_filter, &polyRef, &closestPoint.x);
		if (dtStatusSucceed(dtResult) && polyRef != 0)
		{
			out_distance = dtVdist(&closestPoint.x, &point.x);
			return polyRef;
		}

		// still nothing ..
		// try with bigger search box
		extents[1] = 200.0f;
		dtResult = m_navQuery->findNearestPoly(&point.x, &extents.x, &m_filter, &polyRef, &closestPoint.x);
		if (dtStatusSucceed(dtResult) && polyRef != 0)
		{
			out_distance = dtVdist(&closestPoint.x, &point.x);
			return polyRef;
		}

		return 0;
	}

	namespace
	{
		static float frand()
		{
			return (float)rand() / (float)RAND_MAX;
		}
	}

	bool Map::getRandomPointOnGround(const math::Vector3 & center, float radius, math::Vector3 & out_point)
	{
		math::Vector3 dtCenter = wowToRecastCoord(center);

		// No nav mesh loaded for this map?
		if (!m_navMesh || !m_navQuery)
		{
			return false;
		}

		int tx, ty;
		m_navMesh->calcTileLoc(&dtCenter.x, &tx, &ty);
		if (!m_navMesh->getTileAt(tx, ty, 0))
		{
			return false;
		}

		float distToStartPoly = 0.0f;
		dtPolyRef startPoly = getPolyByLocation(dtCenter, distToStartPoly);
		dtPolyRef endPoly = 0;

		const bool isFarFromPoly = distToStartPoly > 7.0f;
		if (isFarFromPoly)
		{
			math::Vector3 closestPoint;
			if (dtStatusSucceed(m_navQuery->closestPointOnPoly(startPoly, &dtCenter.x, &closestPoint.x, nullptr)))
			{
				dtCenter = closestPoint;
			}
		}

		math::Vector3 out;
		dtStatus dtResult = m_navQuery->findRandomPointAroundCircle(startPoly, &dtCenter.x, radius, &m_adtSlopeFilter, frand, &endPoly, &out.x);
		if (dtStatusSucceed(dtResult))
		{
			out_point = recastToWoWCoord(out);
			return true;
		}

		return false;
	}

	std::shared_ptr<math::AABBTree> Map::getWMOTree(const String & filename) const
	{
		auto it = aabbTreeById.find(filename);
		if (it == aabbTreeById.end())
			return nullptr;

		return it->second;
	}

	std::shared_ptr<math::AABBTree> Map::getDoodadTree(const String &filename) const
	{
		auto it = aabbDoodadTreeById.find(filename);
		if (it == aabbDoodadTreeById.end())
			return nullptr;

		return it->second;
	}

	Map::MapDataTilePtr Map::loadTile(const TileIndex2D & tileIndex)
	{
		std::ostringstream strm;
		strm << m_dataPath.string() << "/maps/" << m_entry.id() << "/" << tileIndex[0] << "_" << tileIndex[1] << ".map";

		const String file = strm.str();
		if (!boost::filesystem::exists(file))
		{
			// File does not exist
			//DLOG("Could not load map file " << file << ": File does not exist");
			return nullptr;
		}

		// Open file for reading
		std::ifstream mapFile(file.c_str(), std::ios::in | std::ios::binary);
		if (!mapFile)
		{
			ELOG("Could not load map file " << file);
			return nullptr;
		}

		// Create reader object
		io::StreamSource fileSource(mapFile);
		io::Reader reader(fileSource);

		// Read map header
		MapHeaderChunk mapHeaderChunk;
		reader.readPOD(mapHeaderChunk);
		if (mapHeaderChunk.header.fourCC != MapHeaderChunkCC)
		{
			ELOG("Could not load map file " << file << ": Invalid four-cc code!");
			return nullptr;
		}
		if (mapHeaderChunk.header.size != sizeof(MapHeaderChunk) - 8)
		{
			ELOG("Could not load map file " << file << ": Unexpected header chunk size (" << (sizeof(MapHeaderChunk) - 8) << " expected)!");
			return nullptr;
		}
		if (mapHeaderChunk.version != MapHeaderChunk::MapFormat)
		{
			ELOG("Could not load map file " << file << ": Unsupported file format version!");
			return nullptr;
		}

		// Allocate tile data
		m_tiles(tileIndex[0], tileIndex[1]) = std::make_shared<MapDataTile>();
		auto tile = m_tiles(tileIndex[0], tileIndex[1]);

		// Read area table
		fileSource.seek(mapHeaderChunk.offsAreaTable);
		reader.readPOD(tile->areas);
		if (tile->areas.header.fourCC != MapAreaChunkCC || tile->areas.header.size != sizeof(MapAreaChunk) - sizeof(MapChunkHeader))
		{
			WLOG("Map file " << file << " seems to be corrupted: Wrong area chunk");
			return nullptr;
		}

		// Read wmos for line of sight checks
		if (mapHeaderChunk.offsWmos)
		{
			fileSource.seek(mapHeaderChunk.offsWmos);
			reader.readPOD(tile->wmos.header);
			if (tile->wmos.header.fourCC != MapWMOChunkCC)
			{
				WLOG("Map file " << file << " seems to be corrupted: Wrong wmo chunk header");
				return nullptr;
			}

			UInt32 wmoCount = 0;
			reader >> io::read<UInt32>(wmoCount);

			tile->wmos.entries.resize(wmoCount);
			for (auto &wmo : tile->wmos.entries)
			{
				reader
					>> io::read<NetUInt32>(wmo.uniqueId)
					>> io::read_container<UInt16>(wmo.fileName);
				reader.readPOD(wmo.inverse);
				reader.readPOD(wmo.bounds);

				// Load aabbtree
				auto it = aabbTreeById.find(wmo.fileName);
				if (it == aabbTreeById.end())
				{
					auto treeFilePath = m_dataPath / "bvh" / (wmo.fileName + ".bvh");

					std::ifstream bvhFile(treeFilePath.string().c_str(), std::ios::in | std::ios::binary);
					if (!bvhFile)
					{
						ELOG("Could not load bvh file " << treeFilePath.string());
						return nullptr;
					}

					// Create reader object
					io::StreamSource bvhSrc(bvhFile);
					io::Reader bvhRead(bvhSrc);

					// Read bvh tree data from file
					auto tree = std::make_shared<math::AABBTree>();
					bvhRead >> *tree;

					// Check if empty
					if (tree->getIndices().empty())
					{
						WLOG("BVH tree " << wmo.fileName << " has no triangles (empty)!");
					}

					// Store tree
					aabbTreeById[wmo.fileName] = tree;
				}
			}
		}

		// Read doodad chunks for editor serialization
		if (mapHeaderChunk.offsDoodads && m_loadDoodads)
		{
			fileSource.seek(mapHeaderChunk.offsDoodads);
			reader.readPOD(tile->doodads.header);
			if (tile->doodads.header.fourCC != MapDoodadChunkCC)
			{
				WLOG("Map file " << file << " seems to be corrupted: Wrong doodad chunk header");
				return nullptr;
			}

			UInt32 doodadCount = 0;
			reader >> io::read<UInt32>(doodadCount);

			tile->doodads.entries.resize(doodadCount);
			for (auto &doodad : tile->doodads.entries)
			{
				reader
					>> io::read<NetUInt32>(doodad.uniqueId)
					>> io::read_container<UInt16>(doodad.fileName);
				reader.readPOD(doodad.inverse);
				reader.readPOD(doodad.bounds);

				// Load aabbtree
				auto it = aabbDoodadTreeById.find(doodad.fileName);
				if (it == aabbDoodadTreeById.end())
				{
					auto treeFilePath = m_dataPath / "bvh" / (doodad.fileName + ".bvh");

					std::ifstream bvhFile(treeFilePath.string().c_str(), std::ios::in | std::ios::binary);
					if (!bvhFile)
					{
						return nullptr;
					}

					// Create reader object
					io::StreamSource bvhSrc(bvhFile);
					io::Reader bvhRead(bvhSrc);

					// Read bvh tree data from file
					auto tree = std::make_shared<math::AABBTree>();
					bvhRead >> *tree;

					// Check if empty
					if (tree->getIndices().empty())
					{
						WLOG("BVH tree " << doodad.fileName << " has no triangles (empty)!");
					}

					// Store tree
					aabbDoodadTreeById[doodad.fileName] = tree;
				}
			}
		}

		// Read navigation data
		if (m_navMesh && mapHeaderChunk.offsNavigation)
		{
			fileSource.seek(mapHeaderChunk.offsNavigation);
			reader.readPOD(tile->navigation.header);
			if (tile->navigation.header.fourCC != MapNavChunkCC)
			{
				WLOG("Map file " << file << " seems to be corrupted: Wrong nav chunk header chunk");
				return nullptr;
			}

			// Read navigation meshes if any
			reader >> io::read<UInt32>(tile->navigation.tileCount);
			tile->navigation.tiles.resize(tile->navigation.tileCount);
			for (UInt32 i = 0; i < tile->navigation.tileCount; ++i)
			{
				auto &data = tile->navigation.tiles[i];
				fileSource.read(reinterpret_cast<char *>(&data.size), sizeof(UInt32));

				// Finally read tile data
				if (data.size)
				{
					// Reserver and read
					data.data.resize(data.size);
					fileSource.read(data.data.data(), data.size);

					// Add tile to navmesh
					dtTileRef ref = 0;
					dtStatus status = m_navMesh->addTile(reinterpret_cast<unsigned char*>(data.data.data()),
						data.data.size(), 0, 0, &ref);
					if (dtStatusFailed(status))
					{
						ELOG("Failed adding nav tile at " << tileIndex << ": 0x" << std::hex << (status & DT_STATUS_DETAIL_MASK));
					}
				}
			}
		}

		return tile;
	}

	Vertex recastToWoWCoord(const Vertex & in_recastCoord)
	{
		return Vertex(-in_recastCoord.z, -in_recastCoord.x, in_recastCoord.y);
	}

	Vertex wowToRecastCoord(const Vertex & in_wowCoord)
	{
		return Vertex(-in_wowCoord.y, in_wowCoord.z, -in_wowCoord.x);
	}
}
