// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <build/util/TileCatalog.h>
#include <clarisma/data/HashMap.h>
#include <geodesk/feature/FeatureType.h>
#include <geodesk/feature/Tip.h>

using namespace geodesk;

class SearchScope 
{
public:
    SearchScope(const TileCatalog& tileCatalog) :
        tileCatalog_(tileCatalog) {}

    enum Flags
    {
        SEARCH_NODES = 1 << 0,
        SEARCH_WAYS = 1 << 1,
        SEARCH_RELATIONS = 1 << 2,
        SEARCH_PARENT_TILES = 1 << 3,
        CHECK_NODE_CHANGES = 1 << 4,
        CHECK_WAY_CHANGES = 1 << 5,
        CHECK_RELATION_CHANGES = 1 << 6
    };

    void checkNodes(int pile)
    {
        setCascadingFlags(pile, SEARCH_NODES | CHECK_NODE_CHANGES);
    }

    void checkWays(int pilePair)
    {
        setPairFlags(pilePair, SEARCH_WAYS | CHECK_WAY_CHANGES);
    }

    void checkRelations(int pilePair)
    {
        setPairFlags(pilePair, SEARCH_RELATIONS | CHECK_RELATION_CHANGES);
    }

    void searchFeatures(FeatureType type, int pileOrPair)
    {
        if (type == FeatureType::NODE)
        {
            setCascadingFlags(pileOrPair, SEARCH_NODES);
        }
        else
        {
            setPairFlags(pileOrPair, type == FeatureType::WAY ?
                SEARCH_WAYS : SEARCH_RELATIONS);
        }
    }

private:
    void setCascadingFlags(int pile, int newFlags)
    {
        assert(pile > 0 && pile <= tileCatalog_.tileCount());

        int& flags = tiles_[pile];
        int prevFlags = flags;
        flags |= newFlags;
        if ((prevFlags & SEARCH_PARENT_TILES) == 0)
        {
            flags |= SEARCH_PARENT_TILES;
            Tile tile = tileCatalog_.tileOfPile(pile);
            if (tile.zoom() == 0) return;
            tile = tile.zoomedOut(tileCatalog_.levels().parentZoom(tile.zoom()));
            pile = tileCatalog_.pileOfTile(tile);
            assert(pile);
            setCascadingFlags(pile, newFlags);
        }
    }

    void setPairFlags(int pilePair, int newFlags)
    {
        int pile = pilePair >> 2;
        assert(pile > 0 && pile <= tileCatalog_.tileCount());
        tiles_[pile] |= newFlags;
        if (pilePair & 3)   [[unlikely]]
        {
            TilePair tilePair = tileCatalog_.tilePairOfPilePair(pilePair);
            pile = tileCatalog_.pileOfTile(tilePair.second());
            assert(pile > 0 && pile <= tileCatalog_.tileCount());
            tiles_[pile] |= newFlags;
        }
    }

    clarisma::HashMap<int,int> tiles_;
    const TileCatalog& tileCatalog_;
};
