// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/alloc/Block.h>
#include <clarisma/io/File.h>
#include <clarisma/thread/TaskEngine.h>
#include <geodesk/feature/FeatureStore_Transaction.h>
#include <geodesk/feature/Tip.h>
#include <geodesk/geom/Tile.h>
#include "tile/model/TileModel.h"
#include "tile/tes/TesArchive.h"
#include "tile/tes/TesParcel.h"
#include "tile/util/TileData.h"

namespace geodesk {
class FeatureStore;
}

class TileLoader;

class TileLoaderTask
{
public:
	TileLoaderTask() {} // TODO: only to satisfy compiler
	TileLoaderTask(Tip tip, Tile tile) : tip_(tip), tile_(tile) {}
	TileLoaderTask(Tip tip, Tile tile, ByteBlock data) :
		tip_(tip), 
		tile_(tile),
		data_(std::move(data))
	{
	}

	Tile tile() const { return tile_; }
	Tip tip() const { return tip_; }
	const uint8_t* data() const { return data_.data(); }
	size_t size() const { return data_.size(); }

private:
	Tip tip_;
	Tile tile_;
	ByteBlock data_;
};


class TileLoaderWorker
{
public:
	explicit TileLoaderWorker(TileLoader* loader) : loader_(loader) {}
	void processTask(TileLoaderTask& task);
	void afterTasks() {}
	void harvestResults() {}

private:
	TileLoader* loader_;
};


class TileLoader : public TaskEngine<TileLoader, TileLoaderWorker, TileLoaderTask, TileData>
{
public:
	TileLoader(FeatureStore* store, int numberOfThreads);

	void load(const char *golFileName, const char *gobFileName, bool wayNodeIds);
	void processTask(TileData& task);
	int64_t totalBytesWritten() const { return totalBytesWritten_; }
	void reportSuccess(int tileCount);

private:
	void initStore(const TesArchiveHeader& header, ByteBlock&& compressedMetadata);

	void verifyHeader(const TesArchiveHeader& header);
	int determineTiles();

	FeatureStore::Transaction transaction_;
	double workPerTile_;
	double workCompleted_;
	size_t totalBytesWritten_;
	size_t bytesSinceLastCommit_;
	size_t headerAndCatalogSize_ = 0;
	File file_;
	std::unique_ptr<std::byte> catalog_;
	std::unique_ptr<Tile[]> tiles_;
	Box bounds_ = Box::ofWorld();
	Filter* filter_ = nullptr;
	bool wayNodeIds_ = false;

#ifdef _DEBUG
	ElementCounts totalCounts_;
	std::mutex counterMutex_;
	void addCounts(const ElementCounts subTotal);
#endif
	friend class TileLoaderWorker;
};


