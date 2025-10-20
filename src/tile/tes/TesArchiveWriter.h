// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/alloc/Block.h>
#include <clarisma/io/File.h>
#include <clarisma/util/DateTime.h>
#include "tile/tes/TesArchive.h"
#include "tile/util/TileData.h"

class TesArchiveWriter 
{
public:
    void open(const char* fileName, const clarisma::UUID& guid, uint32_t revision,
        clarisma::DateTime timestamp, int tileCount, bool wayNodeIds);
    void writeMetadata(TileData&& data);
    void writeTile(TileData&& data);
    void close();

    static TileData createTes(Tip tip, clarisma::ByteBlock&& block);

private:
    std::unique_ptr<std::byte> catalog_;
    TesArchiveEntry* pNextEntry_ = nullptr;
    uint32_t catalogPayloadSize_ = 0;
    clarisma::File out_;
    const char* fileName_ = nullptr;
    std::string tempFileName_;
};
