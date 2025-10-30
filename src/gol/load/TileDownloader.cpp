// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileDownloader.h"
#include "TileDownloadClient.h"

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
    TileDownloadClient mainClient(*this, svUrl);
    mainClient.download();
    dumpRanges();
    mainClient.downloadRanges();

    end();
    transaction_.commit();
    transaction_.end();

    // TODO: only display "Done" if tiles were downloaded
    Console::end().success() << "Done.\n";
}

void TileDownloader::determineRanges(TileDownloadClient& mainClient, bool loadedMetadata)
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
                    mainClient.setRange(pRangeStart, pRangeEnd);
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
        mainClient.setRange(pRangeStart, pRangeEnd);
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