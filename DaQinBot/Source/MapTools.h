#pragma once

#include <BWTA.h>
#include <vector>

#include "Common.h"
#include "DistanceMap.h"

// Keep track of map information, like what tiles are walkable or buildable.

namespace UAlbertaBot
{

struct ChokeData
{
    int width;

    bool isRamp;
    BWAPI::TilePosition highElevationTile;

    bool requiresMineralWalk;
    BWAPI::Unit firstMineralPatch;
    BWAPI::Unit secondMineralPatch;

    ChokeData(const BWEM::ChokePoint* choke)
        : width(0)
        , isRamp(false)
        , highElevationTile(BWAPI::TilePosition(choke->Center()))
        , requiresMineralWalk(false)
        , firstMineralPatch(nullptr)
        , secondMineralPatch(nullptr)
    {};
};

class MapTools
{
	const size_t allMapsSize = 40;			// store this many distance maps in _allMaps

	std::map<BWAPI::TilePosition, DistanceMap>
						_allMaps;			// a cache of already computed distance maps
	std::vector< std::vector<bool> >
						_terrainWalkable;	// walkable considering terrain only
	std::vector< std::vector<bool> >
						_walkable;			// walkable considering terrain and neutral units
	std::vector< std::vector<bool> >
						_buildable;
	std::vector< std::vector<bool> >
						_depotBuildable;
	bool				_hasIslandBases;

    MapTools();

    void				setBWAPIMapData();					// reads in the map data from bwapi and stores it in our map format

	BWTA::BaseLocation *nextExpansion(bool hidden, bool wantMinerals, bool wantGas);

public:

    static MapTools &	Instance();

	int		getGroundTileDistance(BWAPI::TilePosition from, BWAPI::TilePosition to);
	int		getGroundTileDistance(BWAPI::Position from, BWAPI::Position to);
	int		getGroundDistance(BWAPI::Position from, BWAPI::Position to);

    int     closestBaseDistance(BWTA::BaseLocation * base, std::vector<BWTA::BaseLocation*> bases);

	// Pass only valid tiles to these routines!
	bool	isTerrainWalkable(BWAPI::TilePosition tile) const { return _terrainWalkable[tile.x][tile.y]; };
	bool	isWalkable(BWAPI::TilePosition tile) const { return _walkable[tile.x][tile.y]; };
	bool	isBuildable(BWAPI::TilePosition tile) const { return _buildable[tile.x][tile.y]; };
	bool	isDepotBuildable(BWAPI::TilePosition tile) const { return _depotBuildable[tile.x][tile.y]; };

	bool	isBuildable(BWAPI::TilePosition tile, BWAPI::UnitType type) const;

	const std::vector<BWAPI::TilePosition> & getClosestTilesTo(BWAPI::TilePosition pos);
	const std::vector<BWAPI::TilePosition> & getClosestTilesTo(BWAPI::Position pos);

	void	drawHomeDistanceMap();

	BWAPI::TilePosition	getNextExpansion(bool hidden, bool wantMinerals, bool wantGas);

	BWAPI::Position     getDistancePosition(BWAPI::Position start, BWAPI::Position end, double dist);
	BWAPI::Position     getExtendedPosition(BWAPI::Position start, BWAPI::Position end, double dist);

	// center---圆心坐标， radius---圆半径， sp---圆外一点， rp1,rp2---切点坐标   
	void				getCutPoint(BWAPI::Position center, double radius, BWAPI::Position sp, BWAPI::Position & rp1, BWAPI::Position & rp2);

	//获取指定出生点位的外范围坐标集
	std::vector<BWAPI::Position> calculateEnemyRegionVertices(BWTA::BaseLocation * baseLocation);

	bool	hasIslandBases() const { return _hasIslandBases; };
};

}