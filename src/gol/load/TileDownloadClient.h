// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/alloc/Block.h>
#include <clarisma/net/HttpClient.h>
#include <clarisma/net/HttpResponseReader.h>
#include "tile/tes/TesArchive.h"

using namespace clarisma;

class TileLoader;

class TileDownloadClient : public HttpResponseReader<TileDownloadClient>
{
public:
	TileDownloadClient(TileLoader& loader, const std::string_view& url) :
		loader_(loader),
		client_(url)
	{
		// client_.setUserAgent("gol/" GEODESK_GOL_VERSION);
		// TODO!!!
		client_.client().set_keep_alive(true);       // ensure we don't send Connection: close
		client_.client().set_tcp_nodelay(true);      // avoid Nagle delays on small requests
		client_.client().set_connection_timeout(5);  // connect() only
		//client_.setWriteTimeout(60);
		//client_.setReadTimeout(60);
	}

	HttpClient* client() { return &client_; }	// CRTP override

	void download();
	void downloadRanges();
	bool acceptResponse(int status, const HttpResponseHeaders& headers);  // CRTP override

	void setRange(const TesArchiveEntry* pStart, const TesArchiveEntry* pEnd)
	{
		pCurrentTile_ = pStart;
		pEndTile_ = pEnd;
	}

	std::string_view etag() const { return etag_; }
	void setEtag(std::string_view etag) { etag_ = etag; }

private:
	bool processHeader();
	bool processCatalog();
	bool processMetadata();
	bool skipMetadata();
	bool processTile();
	bool skipTile();
	bool nextTile();

	TileLoader& loader_;
	HttpClient client_;
	ByteBlock compressed_;
	const TesArchiveEntry* pCurrentTile_ = nullptr;
	const TesArchiveEntry* pEndTile_ = nullptr;
	std::string etag_;
};

