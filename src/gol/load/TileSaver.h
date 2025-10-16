// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/io/File.h>
#include <clarisma/thread/TaskEngine.h>
#include <geodesk/feature/Tip.h>
#include <geodesk/geom/Tile.h>
#include "tile/tes/TesArchive.h"
#include "tile/tes/TesArchiveWriter.h"
#include "tile/tes/TesWriter.h"

namespace geodesk {
class FeatureStore;
class ZoomLevels;
}
class TileSaver;


class TileSaverTask
{
public:
	TileSaverTask() {} // TODO: only to satisfy compiler
	TileSaverTask(Tile tile, Tip tip) : tile_(tile), tip_(tip) {}

	Tile tile() const { return tile_; }
	Tip tip() const { return tip_; }

private:
	Tile tile_;
	Tip tip_;
};


class TileSaverWorker
{
public:
	explicit TileSaverWorker(TileSaver* saver) : saver_(saver) {}
	void processTask(TileSaverTask& task);
	void afterTasks() {}
	void harvestResults() {}

private:
	TileSaver* saver_;
};

class TileSaver : public TaskEngine<TileSaver, TileSaverWorker, TileSaverTask, TileData>
{
public:
	TileSaver(FeatureStore* store, int threadCount);

	void save(const char* fileName, std::vector<std::pair<Tile,Tip>>& tiles, bool wayNodeIds);
	void preProcessOutput();     // CRTP override
	void processTask(TileData& task);
	int64_t totalBytesWritten() const { return totalBytesWritten_; }

	static TileData compressTile(Tip tip, ByteBlock&& data);

private:
	ByteBlock gatherMetadata() const;
	static void writeMetadataSection(uint8_t*& p, TesMetadataType type, const void* src, size_t size);
	ByteBlock createBlankTileIndex() const;

	FeatureStore* store_;
	TesArchiveWriter writer_;
	double workPerTile_;
	double workCompleted_;
	int64_t totalBytesWritten_;
	int entryCount_;
	bool wayNodeIds_ = false;

	friend class TileSaverWorker;
};


