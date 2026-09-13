// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "geodesk/feature/FeatureStore.h"
#include "tile/model/TileModel.h"

namespace geodesk {
class FeatureStore;
}

using namespace geodesk;

struct TesArchiveEntry;

class TileCompiler 
{
public:
    explicit TileCompiler(FeatureStore* store) :
        store_(store)
    {
        tile_.wayNodeIds(store->hasWaynodeIds());
    }

    void createTile(Tile tile, size_t estimatedTileSize)
    {
        tile_.init(tile, estimatedTileSize);
    }

    void modifyTile(Tip tip, Tile tile);
    void addChanges(std::span<const uint8_t> tesData);
    void addChanges(const TesArchiveEntry& entry, const uint8_t* data);
    ByteBlock compile();

private:
    FeatureStore* store_;
    TileModel tile_;
};
