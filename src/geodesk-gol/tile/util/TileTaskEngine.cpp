// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileTaskEngine.h"
#include <clarisma/cli/Console.h>
#include <geodesk/query/TileIndexWalker.h>

TileTaskEngine::TileTaskEngine(FeatureStore& store, int threadCount) :
	TaskEngine(threadCount),
	store_(store),
	workCompleted_(0),
	workPerTile_(0)
{
}


void TileTaskEngine::run()
{
	std::vector<std::pair<Tip, Tile>> tiles;

	Console::get()->start("");

	// TODO: restrict based on area, bbox or tileset
	//  Also skip missing/stale tiles
	TileIndexWalker tiw(store_.tileIndex(), store_.zoomLevels(), Box::ofWorld(), nullptr);
	do
	{
		// TODO: Check if this works for all subclasses
		if(!tiw.currentEntry().isLoadedAndCurrent()) [[unlikely]]
		{
			continue;
		}
		tiles.emplace_back( tiw.currentTip(), tiw.currentTile() );
	}
	while (tiw.next());

	workPerTile_ = 100.0 / tiles.size();
	workCompleted_ = 0;

	preProcess();		// TODO: This could be done on the output thread
	start();
	for (const auto& tile : tiles)
	{
		prepareTile(tile.first, tile.second);
		postWork(TileTask(tile.first, tile.second));
	}
	end();
}


void TileTaskEngine::processTask(TileOutputTask& task)
{
	processOutput(task.tip(), std::move(task.data()));
	workCompleted_ += workPerTile_;
	Console::get()->setProgress(static_cast<int>(workCompleted_));
}


void TileTaskContext::processTask(TileTask& task)  // CRTP override
{
	engine_->processTile(task.tip(), task.tile());
}