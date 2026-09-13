// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <span>
#include <clarisma/io/FileHandle.h>
#include <geodesk/feature/FeatureStore.h>

using namespace geodesk;

class TileSizeCollector
{
public:
    TileSizeCollector(FeatureStore& store, std::span<uint64_t> tiles) :
        tiles_(tiles),
        store_(store) {}

    void collect();

private:
    void worker();

    std::span<uint64_t> tiles_;
    FeatureStore& store_;
    std::atomic<int> cursor{0};
    std::atomic<clarisma::FileError> error_ = clarisma::FileError::OK;
};

