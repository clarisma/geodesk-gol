// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <clarisma/data/HashMap.h>
#include <geodesk/feature/Tip.h>
#include <geodesk/feature/ZoomLevels.h>
#include <geodesk/geom/Tile.h>
#include <geodesk/geom/TilePair.h>

namespace geodesk {
class FeatureStore;
}
class TileIndexBuilder;

using namespace geodesk;


class TileCatalog
{
public:
	TileCatalog() :	tileCount_(0) {}
	explicit TileCatalog(FeatureStore* store);

	static const int MAX_ZOOM = 12;

	void build(TileIndexBuilder& builder);

	int tileCount() const { return tileCount_; }

	ZoomLevels levels() const { return levels_; }

	Tile tileOfPile(int pile) const
	{
		assert(pile > 0 && pile <= tileCount_); // pile is 1-based
		return pileToTile_[pile];
	}

	Tile tileOfTip(Tip tip) const
	{
		return tileOfPile(pileOfTip(tip));
	}

	Tip tipOfPile(int pile) const
	{
		assert(pile > 0 && pile <= tileCount_); // pile is 1-based
		return pileToTip_[pile];
	}

	int pileOfTip(Tip tip) const
	{
		// TODO: assert range
		assert(tipToPile_[tip] > 0); // Not all TIPs are valid; pile numbers are 1-based
		return tipToPile_[tip];
	}

	Tip tipOfTile(Tile tile) const
	{
		return pileToTip_[pileOfTile(tile)];
	}

	TilePair tilePairOfPilePair(int pilePair) const
	{
		int firstPile = pilePair >> 2;
		assert(firstPile > 0 && firstPile <= tileCount_); // pile is 1-based
		TilePair pair(pileToTile_[firstPile]);
		pair.extend(pilePair & 3);
		return pair;
	}

	int pileOfCoordinate(Coordinate c) const
	{
		int col = Tile::columnFromXZ(c.x, MAX_ZOOM);
		int row = Tile::rowFromYZ(c.y, MAX_ZOOM);
		// printf("12/%d/%d\n", col, row);
		assert(cellToPile_[cellOf(col, row)] == pileOfTileOrParent(
			Tile::fromColumnRowZoom(col, row, MAX_ZOOM)));
		return cellToPile_[cellOf(col, row)];
	}

	int pileOfCoordinateSlow(Coordinate c) const
	{
		int col = Tile::columnFromXZ(c.x, MAX_ZOOM);
		int row = Tile::rowFromYZ(c.y, MAX_ZOOM);
		Tile tile = Tile::fromColumnRowZoom(col, row, MAX_ZOOM);
		for(;;)
		{
			auto it = tileToPile_.find(tile);
			if(it != tileToPile_.end()) return it->second;
			tile = tile.zoomedOut(levels_.parentZoom(tile.zoom()));
		}
	}

	Tip tipOfCoordinateSlow(Coordinate c) const
	{
		return tipOfPile(pileOfCoordinateSlow(c));
	}

	Tile tileOfCoordinateSlow(Coordinate c) const
	{
		// TODO: could avoid array lookup
		return tileOfPile(pileOfCoordinateSlow(c));
	}

	int pileOfTile(Tile tile) const noexcept
	{
		auto it = tileToPile_.find(tile);
		return (it != tileToPile_.end()) ? it->second : 0;
	}

	int pilePairOfTilePair(TilePair tilePair) const noexcept
	{
		auto it = tileToPile_.find(tilePair.first());
		return (it != tileToPile_.end()) ?
			((it->second << 2) |
				(static_cast<uint32_t>(tilePair) >> TilePair::EXTENDS_EAST_BIT)) : 0;
	}

	TilePair normalizedTilePair(TilePair pair) const noexcept;
	// std::pair<int,int> pilesOfTilePairOrParent(TilePair pair) const noexcept;
	int pileOfTileOrParent(Tile tile) const noexcept;
	void write(std::filesystem::path path) const;

private:
	static constexpr int cellOf(int col, int row)
	{
		assert(col >= 0 && col < (1 << MAX_ZOOM));
		assert(row >= 0 && row < (1 << MAX_ZOOM));
		return row * (1 << MAX_ZOOM) + col;
	}

	std::unique_ptr<const int[]> cellToPile_;
	std::unique_ptr<const int[]> tipToPile_;
	std::unique_ptr<const Tile[]> pileToTile_;
	std::unique_ptr<const Tip[]> pileToTip_;
	clarisma::HashMap<Tile, int> tileToPile_;
	int tileCount_;
	ZoomLevels levels_;
};
