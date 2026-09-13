// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <vector>
#include <clarisma/data/HashMap.h>
#include <geodesk/feature/TilePtr.h>
#include "ChangedFeatureBase.h"

namespace clarisma {
class BufferWriter;
}

/// Manages TEX changes within a tile
///
class TexAssigner
{
public:
    void needsTex(int32_t handle, ChangedFeatureBase* feature)
    {
        texChanges_[handle] = feature;
    }

    void losesTex(int32_t handle)
    {
        texChanges_[handle] = nullptr;
    }
    void assign(Tip tip, TilePtr tilePtr);
    void writeChanges(clarisma::BufferWriter& out);

private:
    struct ExportTableEntry
    {
        ExportTableEntry(Tex tex, bool changed, ChangedFeatureBase* feature) :
            tex(tex), changed(changed), feature(feature) {}

        Tex tex;
        bool changed;
        ChangedFeatureBase* feature;
    };

    clarisma::HashMap<int32_t,ChangedFeatureBase*> texChanges_;
        // Contains features that will need a TEX (though they
        // may already have one) and features that will no
        // longer have a TEX (in this case, the pointer is null)
        // Key is the offset of the feature within the tile
    std::vector<ExportTableEntry> exportTableChanges_;
        // Concrete changes to the tile's export table
};

