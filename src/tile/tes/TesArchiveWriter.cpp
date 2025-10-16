// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TesArchiveWriter.h"
#include <clarisma/util/Strings.h>
#include <clarisma/zip/Zip.h>

#include "clarisma/util/Crc32C.h"
#include "clarisma/util/log.h"

using namespace clarisma;

/*
TesArchiveWriter::TesArchiveWriter()
{
}
 */


void TesArchiveWriter::open(const char* fileName, const clarisma::UUID& guid, uint32_t revision,
    DateTime timestamp, int tileCount, bool wayNodeIds)
{
    fileName_ = fileName;
    tempFileName_ = Strings::combine(fileName, ".tmp");
    catalogPayloadSize_ = sizeof(TesArchiveHeader) +
        sizeof(TesArchiveEntry) * tileCount;
    size_t catalogSize = catalogPayloadSize_ + sizeof(uint32_t);
    catalog_.reset(new byte[catalogSize]);
    out_.open(tempFileName_, File::OpenMode::CREATE |
        File::OpenMode::WRITE | File::OpenMode::TRUNCATE);
        // TODO: NEW vs TRUNCATE based on --overwrite

    TesArchiveHeader* header = reinterpret_cast<TesArchiveHeader*>(catalog_.get());
    new (header) TesArchiveHeader();
    header->guid = guid;
    header->flags = wayNodeIds ? TesArchiveHeader::Flags::WAYNODE_IDS : 0;
    header->revision = revision;
    header->revisionTimestamp = timestamp;
    header->tileCount = tileCount;
    pNextEntry_ = reinterpret_cast<TesArchiveEntry*>(catalog_.get() + sizeof(TesArchiveHeader));
    out_.seek(catalogSize);
        // Skip Header, Entries, checksum
}

void TesArchiveWriter::writeMetadata(TileData&& data)
{
    auto header = reinterpret_cast<TesArchiveHeader*>(catalog_.get());
    header->metadataChunkSize = data.size();
    out_.writeAll(data.data(), data.size());
}

void TesArchiveWriter::writeTile(TileData&& data)
{
    assert(reinterpret_cast<byte*>(pNextEntry_) < catalog_.get() + catalogPayloadSize_);
    *pNextEntry_++ = TesArchiveEntry(data.tip(), data.size());
    out_.writeAll(data.data(), data.size());
    LOGS << "Wrote " << data.size() << " bytes";
}

void TesArchiveWriter::close()
{
    uint32_t* pChecksum = reinterpret_cast<uint32_t*>(catalog_.get() + catalogPayloadSize_);
    *pChecksum = Crc32C::compute(catalog_.get(), catalogPayloadSize_);
    out_.writeAllAt(0, catalog_.get(), catalogPayloadSize_ + sizeof(uint32_t));
    out_.force();
    out_.close();
    File::rename(tempFileName_.c_str(), fileName_);
    fileName_ = nullptr;
    tempFileName_.clear();
}

TileData TesArchiveWriter::createTes(Tip tip, clarisma::ByteBlock&& block)
{
    ByteBlock compressed = Zip::deflateRaw(block);
    uint32_t compressedSize = static_cast<uint32_t>(compressed.size());
    return { tip, compressed.takeData(), compressedSize };
}
