// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tile/util/TileTaskEngine.h"


class GolChecker : public TileTaskEngine
{
public:
    GolChecker(FeatureStore& store, int threadCount) :
        TileTaskEngine(store, threadCount)
    {
    }

    // void prepareTile(Tip tip, Tile tile) override;
    void processTile(Tip tip, Tile tile) override;
};
