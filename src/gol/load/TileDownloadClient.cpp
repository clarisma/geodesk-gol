// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileDownloadClient.h"
#include <clarisma/net/HttpException.h>
#include "TileLoader.h"

void TileDownloadClient::download()
{
    receive(reinterpret_cast<std::byte*>(&loader_.header_),
        sizeof(loader_.header_), &TileDownloadClient::processHeader);
    HttpRequestHeaders headers;
    if (Console::verbosity() >= Console::Verbosity::VERBOSE)
    {
        ConsoleWriter().timestamp() << "Issuing initial request";
    }
    get("", headers);
}

void TileDownloadClient::downloadRanges()
{
    for (;;)
    {
        int next = loader_.nextRange_.fetch_add(1);
        if (next >= loader_.ranges_.size()) break;
        auto [ofs, size, firstEntry, tileCount] = loader_.ranges_[next];
        LOGS << "Requesting " << tileCount << " tile(s) at offset "
            << ofs << " (" << size << " bytes)";
        pCurrentTile_ = loader_.entry(firstEntry);
        pEndTile_ = pCurrentTile_ + tileCount;
        nextTile();
        HttpRequestHeaders headers;
        headers.addRange(ofs, size);
        if (Console::verbosity() >= Console::Verbosity::VERBOSE)
        {
            ConsoleWriter().timestamp() << "Requesting " << tileCount <<
                (tileCount==1 ? " tile at " : " tiles at ")
                << ofs << " (" << size << " bytes)";
        }
        get("", headers);
    }
}

bool TileDownloadClient::acceptResponse(int status, const HttpResponseHeaders& headers)
{
    if (etag_.empty())
    {
        if (status != 200)
        {
            if (status == 404)
            {
                throw HttpException("Tileset not found");
            }
            throw HttpException("Server error %d", status);
        }
        etag_ = headers.etag();
        if (etag_.empty())  [[unlikely]]
        {
            etag_ = "etag";
        }
    }
    else if (status != 206)
    {
        if (status == 200)
        {
            throw HttpException("Server does not support range queries");
        }
        throw HttpException("Server error %d", status);
    }
    if (Console::verbosity() >= Console::Verbosity::VERBOSE)
    {
        ConsoleWriter().timestamp() << "Accepting response";
    }
    return true;
}


bool TileDownloadClient::processHeader()
{
    loader_.prepareCatalog(loader_.header_);
    receive(loader_.catalog_.get() + sizeof(TesArchiveHeader),
        loader_.catalogSize_ - sizeof(TesArchiveHeader),
        &TileDownloadClient::processCatalog);
    return true;
}

bool TileDownloadClient::processCatalog()
{
    loader_.verifyCatalog();
    uint32_t metadataSize = loader_.header_.metadataChunkSize;
    compressed_ = ByteBlock(metadataSize);

    if (loader_.openStore())
    {
        // We've created a fresh store, so we'll need to
        // download the metadata

        receive(reinterpret_cast<std::byte*>(compressed_.data()),
            compressed_.size(),
            &TileDownloadClient::processMetadata);
        return true;
    }
    if (!loader_.beginTiles()) return false;
    loader_.determineRanges(*this, false);

    if (pCurrentTile_ >= pEndTile_)
    {
        // The initial response does not cover any tiles --
        // i.e. we're skipping not only the metadata, but
        // also a large number of tiles at the start of the GOB,
        // so we're better off making one or more range requests
        // to fetch tiles

        return false;
    }

    // Otherwise, we'll continue reading from the initial response;
    // first, we'll skip the metadata chunk

    receive(reinterpret_cast<std::byte*>(compressed_.data()),
            compressed_.size(),
            &TileDownloadClient::skipMetadata);
    return true;
}


bool TileDownloadClient::processMetadata()
{
    loader_.initStore(loader_.header_, std::move(compressed_));
    if (!loader_.beginTiles()) return false;
    loader_.determineRanges(*this, true);
    return nextTile();
}

bool TileDownloadClient::skipMetadata()
{
    return nextTile();
}

bool TileDownloadClient::nextTile()
{
    if (pCurrentTile_ >= pEndTile_) return false;
    compressed_ = ByteBlock(pCurrentTile_->size);
    bool isSkipped = loader_.tiles_[pCurrentTile_->tip].isNull();
    LOGS << "Worker::nextTile: Preparing to " << (isSkipped ? "skip " : "read ")
        << compressed_.size() << " bytes for tile " <<
            loader_.tileOfTip(pCurrentTile_->tip);
    receive(reinterpret_cast<std::byte*>(compressed_.data()),
        compressed_.size(), isSkipped ?
            &TileDownloadClient::skipTile : &TileDownloadClient::processTile);
    return true;
}

bool TileDownloadClient::processTile()
{
    Tip tip = pCurrentTile_->tip;
    loader_.postWork({ tip, loader_.tiles_[tip], std::move(compressed_) });
    pCurrentTile_++;
    return nextTile();
}

bool TileDownloadClient::skipTile()
{
    LOGS << "Skipping " << compressed_.size() << " bytes of tile "
        << pCurrentTile_->tip;
    pCurrentTile_++;
    return nextTile();
}
