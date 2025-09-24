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

namespace geodesk {
class FeatureStore;
}

class TileLoader;

class TileLoaderTask
{
public:
	TileLoaderTask() {} // TODO: only to satisfy compiler
	TileLoaderTask(Tip tip, Tile tile) : tip_(tip), tile_(tile) {}
	TileLoaderTask(Tip tip, Tile tile, TesParcelPtr parcel) : 
		tip_(tip), 
		tile_(tile),
		firstParcel_(std::move(parcel)) 
	{
	}

	Tile tile() const { return tile_; }
	Tip tip() const { return tip_; }
	TesParcelPtr takeFirstParcel() { return std::move(firstParcel_); }

private:
	Tip tip_;
	Tile tile_;
	TesParcelPtr firstParcel_;
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

class TileLoaderOutputTask
{
public:
	TileLoaderOutputTask() {} // TODO: only to satisfy compiler
	TileLoaderOutputTask(int tip, ByteBlock&& data) :
		data_(std::move(data)),
		tip_(tip)
	{}

	Tip tip() const { return tip_; }
	const uint8_t* data() const { return data_.data(); }
	size_t size() const { return data_.size(); }
	
private:
	ByteBlock data_; 
	Tip tip_;
};


class TileLoader : public TaskEngine<TileLoader, TileLoaderWorker, TileLoaderTask, TileLoaderOutputTask>
{
public:
	TileLoader(FeatureStore* store, int numberOfThreads);

	int prepareLoad(const char *tesFileName);
	void load();
	void processTask(TileLoaderOutputTask& task);
	int64_t totalBytesWritten() const { return totalBytesWritten_; }
	void reportSuccess(int tileCount);

private:
	void initStore(const TesArchiveHeader& header,
		ByteBlock&& compressedMetadata, uint32_t sizeUncompressed, uint32_t checksum);

	FeatureStore::Transaction transaction_;
	double workPerTile_;
	double workCompleted_;
	size_t totalBytesWritten_;
	size_t bytesSinceLastCommit_;
	size_t headerAndCatalogSize_ = 0;
	File file_;
	uint32_t entryCount_ = 0;
	std::unique_ptr<TesArchiveEntry[]> catalog_;
	std::unique_ptr<Tile[]> tiles_;
	std::unique_ptr<uint32_t[]> tileIndex_;

#ifdef _DEBUG
	ElementCounts totalCounts_;
	std::mutex counterMutex_;
	void addCounts(const ElementCounts subTotal);
#endif
	friend class TileLoaderWorker;
};


