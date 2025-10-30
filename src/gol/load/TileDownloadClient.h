// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/alloc/Block.h>
#include <clarisma/net/HttpResponseReader.h>
#include "tile/tes/TesArchive.h"

using namespace clarisma;

class TileLoader;
class TileDownloader;

class TileDownloadClient : public HttpResponseReader<TileDownloadClient>
{
public:
	TileDownloadClient(TileDownloader& loader, const std::string_view& url) :
		loader_(loader),
		client_(url)
	{
		// client_.setUserAgent("gol/" GEODESK_GOL_VERSION);
		// TODO!!!
	}

	HttpClient* client() { return &client_; }	// CRTP override

	void download();
	void downloadRanges();
	bool acceptHeaders(const HttpResponseHeaders& headers);  // CRTP override

	void setRange(const TesArchiveEntry* pStart, const TesArchiveEntry* pEnd)
	{
		pCurrentTile_ = pStart;
		pEndTile_ = pEnd;
	}

private:
	bool processHeader();
	bool processCatalog();
	bool processMetadata();
	bool skipMetadata();
	bool processTile();
	bool skipTile();
	bool nextTile();

	TileDownloader& loader_;
	HttpClient client_;
	ByteBlock compressed_;
	const TesArchiveEntry* pCurrentTile_ = nullptr;
	const TesArchiveEntry* pEndTile_ = nullptr;
	std::string etag_;
};

