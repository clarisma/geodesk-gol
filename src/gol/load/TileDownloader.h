// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TileLoader.h"
#include <clarisma/net/HttpResponseReader.h>

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

	class Worker : public HttpResponseReader<Worker>
	{
	public:
		Worker(TileDownloader& downloader, const std::string_view& url) :
			downloader_(downloader),
			client_(url)
		{
			// client_.setUserAgent("gol/" GEODESK_GOL_VERSION);
			// TODO!!!
		}

		HttpClient* client() { return &client_; }	// CRTP override

		void download();
		void downloadRanges();
		bool acceptHeaders(const HttpResponseHeaders& headers);  // CRTP override

	private:
		bool processHeader();
		bool processCatalog();
		bool processMetadata();
		bool skipMetadata();
		bool processTile();
		bool skipTile();
		bool nextTile();

		TileDownloader& downloader_;
		HttpClient client_;
		ByteBlock compressed_;
		const TesArchiveEntry* pCurrentTile_ = nullptr;
		const TesArchiveEntry* pEndTile_ = nullptr;
		std::string etag_;
	};

	void determineRanges(uint64_t skipped);

	const char* url_ = nullptr;
	TesArchiveHeader header_;
	std::vector<Range> ranges_;
	std::atomic<int> nextRange_ = 0;
	uint32_t maxSkippedBytes_ = 1024 * 1024;   // 1 MB
};


