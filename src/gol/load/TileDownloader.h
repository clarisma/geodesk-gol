// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TileLoader.h"
#include <clarisma/net/HttpResponseReader.h>

class TileDownloadClient;

class TileDownloader : public TileLoader
{
public:
	TileDownloader(FeatureStore* store,
		int numberOfThreads) :
		TileLoader(store, numberOfThreads) {}

	void download(const char *golFileName, bool wayNodeIds,
		const char* url, Box bounds, const Filter* filter);

private:
	struct Range
	{
		uint64_t ofs;
		uint64_t size;
		uint32_t firstEntry;
		uint32_t tileCount;
	};

	void determineRanges(TileDownloadClient& mainClient, bool loadedMetadata);
	void dumpRanges();

	const char* url_ = nullptr;
	TesArchiveHeader header_;
	std::vector<Range> ranges_;
	std::atomic<int> nextRange_ = 0;
	uint32_t maxSkippedBytes_ = 1024 * 1024;   // 1 MB

	friend class TileDownloadClient;
};


