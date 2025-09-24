// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <geodesk/feature/Tip.h>

using namespace geodesk;

class TileData
{
public:
    TileData() :
        sizeOriginal_(0),
        sizeCompressed_(0),
        checksum_(0) {}

    TileData(Tip tip, std::unique_ptr<const uint8_t[]> data, uint32_t sizeOriginal,
        uint32_t sizeCompressed = 0, uint32_t checksum = 0) :
        tip_(tip),
        sizeOriginal_(sizeOriginal),
        sizeCompressed_(sizeCompressed),
        checksum_(checksum),
        data_(std::move(data)) {}

    Tip tip() const { return tip_; }
    uint32_t sizeOriginal() const { return sizeOriginal_; }
    uint32_t sizeCompressed() const { return sizeCompressed_; }
    uint32_t checksum() const { return checksum_; }
    const uint8_t* data() const { return data_.get(); }

private:
    Tip tip_;
    uint32_t sizeOriginal_;
    uint32_t sizeCompressed_;
    uint32_t checksum_;
    std::unique_ptr<const uint8_t[]> data_;
};

