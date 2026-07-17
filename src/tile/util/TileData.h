// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <geodesk/feature/Tip.h>

using namespace geodesk;

class TileData
{
public:
    TileData() {}

    TileData(Tip tip, std::unique_ptr<const uint8_t[]> data, uint32_t size) :
        tip_(tip),
        size_(size),
        data_(std::move(data)) {}

    Tip tip() const { return tip_; }
    uint32_t size() const { return size_; }
    const uint8_t* data() const { return data_.get(); }

private:
    Tip tip_;
    uint32_t size_ = 0;
    std::unique_ptr<const uint8_t[]> data_;
};

