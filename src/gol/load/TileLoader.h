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
class TileDownloadClient;

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

	void load(const char *golFileName, const char *gobFileName, bool wayNodeIds,
		Box bounds, const Filter* filter);
	void download(const char *golFileName, bool wayNodeIds,
		const char* url, Box bounds, const Filter* filter);

	void processTask(TileData& task);

private:
	struct Range
	{
		uint64_t ofs;
		uint64_t size;
		uint32_t firstEntry;
		uint32_t tileCount;
	};

	int64_t totalBytesWritten() const { return totalBytesWritten_; }
	void reportSuccess(int tileCount);
	void initStore(const TesArchiveHeader& header, ByteBlock&& compressedMetadata);

	const TesArchiveHeader& gobHeader() const
	{
		return reinterpret_cast<const TesArchiveHeader&>(*catalog_);
	};
	static void verifyHeader(const TesArchiveHeader& header);
	void prepareCatalog(const TesArchiveHeader& header);
	void verifyCatalog() const;
	bool openStore();
	bool beginTiles();
	int determineTiles();
	void determineRanges(TileDownloadClient& mainClient, bool loadedMetadata);
	void dumpRanges();

	const TesArchiveEntry* entry(uint32_t n) const
	{
		return reinterpret_cast<const TesArchiveEntry*>(
			catalog_.get() + sizeof(TesArchiveHeader) +
			n * sizeof(TesArchiveEntry));
	}

	Tile tileOfTip(Tip tip) const { return tiles_[tip]; }

	FeatureStore::Transaction transaction_;
	double workPerTile_;
	double workCompleted_;
	size_t totalBytesWritten_;
	size_t bytesSinceLastCommit_;
	File file_;
	std::unique_ptr<std::byte> catalog_;
	std::unique_ptr<Tile[]> tiles_;
	uint32_t catalogSize_ = 0;
	bool wayNodeIds_ = false;
	bool transactionStarted_ = false;
	const char* golFileName_ = nullptr;
	const char* gobFileName_ = nullptr;
	Box bounds_;
	const Filter* filter_ = nullptr;

	const char* url_ = nullptr;

	// A buffer used for reading the GOB's header
	// (only used if downloading)
	TesArchiveHeader header_;
	std::vector<Range> ranges_;
	std::atomic<int> nextRange_ = 0;

	// When downloading, it makes sense to simply read and discard
	// a range of bytes instead of issuing a separate range request,
	// which incurs latency. This field specifies the threshold
	uint32_t maxSkippedBytes_ = 1024 * 1024;   // 1 MB

	friend class TileDownloadClient;

#ifdef _DEBUG
	ElementCounts totalCounts_;
	std::mutex counterMutex_;
	void addCounts(const ElementCounts subTotal);
#endif
	friend class TileLoaderWorker;
};


