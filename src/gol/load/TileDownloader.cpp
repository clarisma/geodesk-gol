// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileDownloader.h"

void TileDownloader::download(
    const char *golFileName, bool wayNodeIds, const char* url,
    Box bounds, const Filter* filter)
{
    golFileName_ = golFileName;
    gobFileName_ = url; // TODO: consolidate local file & url
    wayNodeIds_ = wayNodeIds;
    url_ = url;
    bounds_ = bounds;
    filter_ = filter;

    Console::get()->start("Downloading...");
    start();
    std::string_view svUrl = url;
    Worker mainWorker(*this, svUrl);
    mainWorker.download();
    dumpRanges();
    mainWorker.downloadRanges();

    end();
    transaction_.commit();
    transaction_.end();

    // TODO: only display "Done" if tiles were downloaded
    Console::end().success() << "Done.\n";
}

void TileDownloader::Worker::download()
{
    receive(reinterpret_cast<std::byte*>(&downloader_.header_),
        sizeof(downloader_.header_), &Worker::processHeader);
    HttpRequestHeaders headers;
    get("", headers);
}

void TileDownloader::Worker::downloadRanges()
{
    for (;;)
    {
        int next = downloader_.nextRange_.fetch_add(1);
        if (next >= downloader_.ranges_.size()) break;
        auto [ofs, size, firstEntry, tileCount] = downloader_.ranges_[next];
        LOGS << "Requesting " << tileCount << " tile(s) at offset "
            << ofs << " (" << size << " bytes)";
        pCurrentTile_ = downloader_.entry(firstEntry);
        pEndTile_ = pCurrentTile_ + tileCount;
        nextTile();
        HttpRequestHeaders headers;
        headers.addRange(ofs, size);
        get("", headers);
    }
}

bool TileDownloader::Worker::acceptHeaders(const HttpResponseHeaders& headers)
{
    int status = headers.status();
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
    return true;
}


bool TileDownloader::Worker::processHeader()
{
    downloader_.prepareCatalog(downloader_.header_);
    receive(downloader_.catalog_.get() + sizeof(TesArchiveHeader),
        downloader_.catalogSize_ - sizeof(TesArchiveHeader),
        &Worker::processCatalog);
    return true;
}

bool TileDownloader::Worker::processCatalog()
{
    downloader_.verifyCatalog();
    uint32_t metadataSize = downloader_.header_.metadataChunkSize;
    if (downloader_.openStore())
    {
        compressed_ = ByteBlock(metadataSize);
        receive(reinterpret_cast<std::byte*>(compressed_.data()),
            compressed_.size(),
            &Worker::processMetadata);
        return true;
    }
    if (!downloader_.beginTiles()) return false;
    downloader_.determineRanges(*this, false);
    return nextTile();
}


bool TileDownloader::Worker::processMetadata()
{
    downloader_.initStore(downloader_.header_, std::move(compressed_));
    if (!downloader_.beginTiles()) return false;
    downloader_.determineRanges(*this, true);
    return nextTile();
}

bool TileDownloader::Worker::skipMetadata()
{
    return nextTile();
}

bool TileDownloader::Worker::nextTile()
{
    if (pCurrentTile_ >= pEndTile_) return false;
    compressed_ = ByteBlock(pCurrentTile_->size);
    bool isSkipped = downloader_.tiles_[pCurrentTile_->tip].isNull();
    LOGS << "Worker::nextTile: Preparing to " << (isSkipped ? "skip " : "read ")
        << compressed_.size() << " bytes for tile " <<
            downloader_.tileOfTip(pCurrentTile_->tip);
    receive(reinterpret_cast<std::byte*>(compressed_.data()),
        compressed_.size(), isSkipped ?
            &Worker::skipTile : &Worker::processTile);
    return true;
}

bool TileDownloader::Worker::processTile()
{
    Tip tip = pCurrentTile_->tip;
    downloader_.postWork({ tip, downloader_.tiles_[tip], std::move(compressed_) });
    pCurrentTile_++;
    return nextTile();
}

bool TileDownloader::Worker::skipTile()
{
    pCurrentTile_++;
    return nextTile();
}


void TileDownloader::determineRanges(Worker& mainWorker, bool loadedMetadata)
{
    uint64_t compressedMetadataSize = header_.metadataChunkSize;
    uint64_t skippedBytes = loadedMetadata ? 0 : compressedMetadataSize;
    uint64_t ofs = catalogSize_ + compressedMetadataSize;
    const TesArchiveEntry* p = reinterpret_cast<const TesArchiveEntry*>(
        catalog_.get() + sizeof(TesArchiveHeader));
    const TesArchiveEntry* pStart = p;
    const TesArchiveEntry* pRangeStart = p;
    const TesArchiveEntry* pRangeEnd = p;
    const TesArchiveEntry* pEnd = p + header_.tileCount;
    uint64_t rangeStartOfs = ofs;
    uint64_t rangeLen = 0;

    while (p < pEnd)
    {
        if (tiles_[p->tip].isNull())
        {
            skippedBytes += p->size;
        }
        else
        {
            if (skippedBytes > maxSkippedBytes_)
            {
                if (pRangeStart == pStart)
                {
                    mainWorker.setRange(pRangeStart, pRangeEnd);
                }
                else
                {
                    ranges_.emplace_back(rangeStartOfs, rangeLen,
                        pRangeStart - pStart,
                        pRangeEnd - pRangeStart);
                }
                rangeStartOfs = ofs;
                rangeLen = 0;
                pRangeStart = p;
            }
            else
            {
                rangeLen += skippedBytes;
            }
            skippedBytes = 0;
            rangeLen += p->size;
            pRangeEnd = p + 1;
        }
        ofs += p->size;
        p++;
    }

    if (pRangeStart == pStart)
    {
        mainWorker.setRange(pRangeStart, pRangeEnd);
    }
    else
    {
        ranges_.emplace_back(rangeStartOfs, rangeLen,
            pRangeStart - pStart,
            pRangeEnd - pRangeStart);
    }
}


void TileDownloader::dumpRanges()
{
    LOGS << ranges_.size() << " ranges:";
    for (Range r : ranges_)
    {
        Tip tip = entry(r.firstEntry)->tip;
        LOGS << "Ofs = " << r.ofs << ", len = " << r.size
            << ", tiles = " << r.tileCount << ", starting at #"
            << r.firstEntry << ": " << tip << " ("
            << tiles_[tip]  << ")";
    }
}