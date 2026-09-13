// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileCatalog.h"
#include <clarisma/cli/Console.h>
#include <geodesk/feature/FeatureStore.h>
#include <geodesk/query/TileIndexWalker.h>
#include "build/analyze/TileIndexBuilder.h"
#include "clarisma/io/FileBuffer3.h"

TileCatalog::TileCatalog(FeatureStore* store) :
	tileCount_(store->tileCount()),
	levels_(store->zoomLevels())
{
	// assert(_CrtCheckMemory());

	DataPtr pTileIndex = store->tileIndex();
	int tipCount = pTileIndex.getInt();
	tileToPile_.reserve(tileCount_);
	std::unique_ptr<int[]> tipToPile(new int[tipCount + 1]);
	memset(tipToPile.get(), 0, (tipCount+1) * sizeof(int));
	std::unique_ptr<Tile[]> pileToTile(new Tile[tileCount_ + 1]);
	std::unique_ptr<Tip[]> pileToTip(new Tip[tileCount_ + 1]);

	TileIndexWalker tiw(pTileIndex, levels_, Box::ofWorld(), nullptr);
	int pile = 0;
	do
	{
		pile++;
		tipToPile[tiw.currentTip()] = pile;
		pileToTile[pile] = tiw.currentTile();
		pileToTip[pile] = tiw.currentTip();
		tileToPile_[tiw.currentTile()] = pile;
	}
	while(tiw.next());

	tipToPile_ = std::move(tipToPile);
	pileToTile_ = std::move(pileToTile);
	pileToTip_ = std::move(pileToTip);
}

void TileCatalog::build(TileIndexBuilder& builder)
{
	tileCount_ = builder.tileCount();
	levels_ = builder.zoomLevels();
	cellToPile_ = builder.takeCellToPile();
	tipToPile_ = builder.takeTipToPile();
	pileToTile_ = builder.takePileToTile();
	pileToTip_ = builder.takePileToTip();

	tileToPile_.reserve(tileCount_);
	for (int i = 1; i <= tileCount_; i++)
	{
		// Console::msg("%d: %s", i, pileToTile_[i].toString().c_str());
		tileToPile_[pileToTile_[i]] = i;
	}
}


int TileCatalog::pileOfTileOrParent(Tile tile) const noexcept
{
	for (;;)
	{
		auto it = tileToPile_.find(tile);
		if (it != tileToPile_.end()) return it->second;
		int zoom = tile.zoom();
		assert(zoom > 0); // root tile must be in tileToPile_
		tile = tile.zoomedOut(levels_.parentZoom(zoom));
	}
}

/*
std::pair<int,int> TileCatalog::pilesOfTilePairOrParent(TilePair pair) const noexcept
{
	for (;;)
	{
		auto it = tileToPile_.find(pair.first());
		if (it != tileToPile_.end())
		{
			auto it2 = tileToPile_.find(pair.second());
			if (it2 != tileToPile_.end())
			{
				return { it->second, it2->second };
				// "second" here means value of the key/value pair
			}
		}
		int zoom = pair.zoom();
		assert(zoom > 0); // root tile must be in tileToPile_
		pair = pair.zoomedOut(levels_.parentZoom(zoom));
	}
}
*/

/*
int TileCatalog::pilePairOfTilePairOrParent(TilePair pair) const noexcept
{
	for (;;)
	{
		auto it = tileToPile_.find(pair.first());
		if (it != tileToPile_.end())
		{
			uint_fast32_t flags = static_cast<uint_fast32_t>(pair) >>
				TilePair::EXTENDS_EAST_BIT;
			return (it->second << 2) | flags;
		}
		int zoom = pair.zoom();
		assert(zoom > 0); // root tile must be in tileToPile_
		pair = pair.zoomedOut(levels_.parentZoom(zoom));
	}
}
*/

TilePair TileCatalog::normalizedTilePair(TilePair pair) const noexcept
{
	return pair.zoomedOut(levels_.parentZoom(pair.zoom() + 1));

	/* // TODO: no longer needed
	int zoom = pair.zoom() + 1;
	for (;;)
	{
		pair = pair.zoomedOut(levels_.parentZoom(zoom));
		if (pileOfTile(pair.first()))
		{
			if (pair.first() == pair.second() || pileOfTile(pair.second()))
			{
				return pair;
			}
		}
		zoom = pair.zoom();
	}
	*/
}


void TileCatalog::write(std::filesystem::path path) const
{
	FileBuffer3 out;
	out.open(path);
	for (int i = 1; i <= tileCount_; i++)
	{
		out << pileToTile_[i] << '\n';
	}
	out.close();
}


