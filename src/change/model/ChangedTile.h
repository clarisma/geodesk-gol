// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/data/LinkedStack.h>
#include "ChangedNode.h"

using namespace geodesk;
using clarisma::LinkedStack;

// TODO: If we arena-allocate this class, we need to call
//  its destructor prior to deallocating the arena, or its members
//  may leak memory

class ChangedTile 
{
public:
    explicit ChangedTile(Tip tip, Tile tile) :
        tip_(tip),
        tile_(tile) {}
        // mayGainTex_(arena) {}

    Tip tip() const { return tip_; }
    clarisma::LinkedStack<ChangedNode>& changedNodes() { return changedNodes_; };
    LinkedStack<ChangedFeatureStub>& changedWays() { return changedWays_; };
    LinkedStack<ChangedFeatureStub>& changedRelations() { return changedRelations_; };
    LinkedStack<ChangedFeatureStub>& deletedNodes() { return deletedNodes_; };
    LinkedStack<ChangedFeatureStub>& deletedWays() { return deletedWays_; };
    LinkedStack<ChangedFeatureStub>& deletedRelations() { return deletedRelations_; };

    void addChanged(ChangedFeatureStub* feature)
    {
        assert(feature->get()->ref().tip() == tip_ ||
            feature->get()->refSE().tip() == tip_);

        if (feature->typedId() == TypedFeatureId::ofWay(89253924))
        {
            LOGS << "Assigning " << feature->typedId() << " to tile " << tip_;
        }
        assert(feature->type() != FeatureType::NODE);
        ((feature->type() == FeatureType::WAY) ? changedWays_ : changedRelations_).push(feature);
    }

    void needsTex(int32_t handle, ChangedFeatureBase* feature)
    {
        texChanges_[handle] = feature;
    }

    void withoutTex(int32_t handle)
    {
        texChanges_[handle] = nullptr;
    }

    // TODO: remove
    void mayGainTex(CFeatureStub* feature)
    {
        // TODO

        assert(feature);
        // mayGainTex_.add(feature);
        // hasTexChanges_ = true;

        // TODO: Unless the feature is marked may_have_tex,
        //  it will definitely need a TEX, hence we could
        //  flag the tile differently to avoid scanning
        //  its exported features to see if any in mayGainTex_
        //  already have a TEX

        // TODO: Idea: use a flag that indicates *all*
        //  features in mayGainTex definitely need a tex,
        //  so we can skip the scan of the export table
        //  to check which features already have a tex
    }

    // bool hasTexChanges() const { return hasTexChanges_; }

    /*
    const ArenaBag<CFeatureStub*,16>& mayGainTex() const
    {
        return mayGainTex_;
    }
    */

    void resolveExports(TilePtr tilePtr);
    void writeChanges(clarisma::BufferWriter& out);

private:
    struct ExportTableEntry
    {
        ExportTableEntry(Tex tex, bool changed, CFeature* feature) :
            tex(tex), changed(changed), feature(feature) {}

        Tex tex;
        bool changed;
        CFeature* feature;
    };

    struct SortedFeature
    {
        SortedFeature(uint32_t hilbert, CFeature* feature) :
            hilbert(hilbert), feature(feature) {}

        /// Orders entries by Hilbert distance.
        bool operator<(const SortedFeature& other) const noexcept
        {
            return hilbert < other.hilbert;
        }

        uint32_t hilbert;
        CFeature* feature;
    };

    static constexpr uint32_t EXPORTS_UNCHANGED = 0xffff'ffff;

    LinkedStack<ChangedNode> changedNodes_;
    LinkedStack<ChangedFeatureStub> changedWays_;
    LinkedStack<ChangedFeatureStub> changedRelations_;
    LinkedStack<ChangedFeatureStub> deletedNodes_;
    LinkedStack<ChangedFeatureStub> deletedWays_;
    LinkedStack<ChangedFeatureStub> deletedRelations_;
    // ArenaBag<CFeatureStub*,16> mayGainTex_;
    Tip tip_;
    Tile tile_;
    // bool hasTexChanges_ = false;

    clarisma::HashMap<int32_t,CFeatureStub*> texChanges_;
        // Contains features that will need a TEX (though they
        // may already have one) and features that will no
        // longer have a TEX (in this case, the pointer is null)
        // Key is the offset of the feature within the tile
        // Uses CFeatureStub*, so we can track unchanged
        // features as well (cannot be CFeature*, because
        // an unchanged feature may become changed, and
        // hence replaces the concrete entry)
    std::vector<ExportTableEntry> exportTableChanges_;
    // Concrete changes to the tile's export table
    uint32_t futureExportsCount_ = EXPORTS_UNCHANGED;
};
