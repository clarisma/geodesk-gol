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
    explicit ChangedTile(Tip tip, Tile tile, TilePtr tilePtr) :
        tip_(tip),
        tile_(tile),
        tilePtr_(tilePtr) {}

    Tip tip() const { return tip_; }

    void dump() const;

    // TODO: Change all to ChangedFeatureStub;
    //  node can be copied as well, because it may be added in one
    //  tile and deleted in another!
    //  No, original is always stored in changedNodes,
    //   copy goes into deletedNodes
    //   --> Then allow consumers to cast to ChangedNode, since that
    //       is always safe!
    LinkedStack<ChangedNode>& changedNodes() { return changedNodes_; };
    LinkedStack<ChangedFeatureStub>& changedWays() { return changedWays_; };
    LinkedStack<ChangedFeatureStub>& changedRelations() { return changedRelations_; };
    LinkedStack<ChangedFeatureStub>& deletedFeatures(FeatureType type)
    {
        return deleted_[static_cast<int>(type)];
    }
    LinkedStack<ChangedFeatureStub>& deletedNodes()
    {
        return deletedFeatures(FeatureType::NODE);
    }
    LinkedStack<ChangedFeatureStub>& deletedWays()
    {
        return deletedFeatures(FeatureType::WAY);
    }
    LinkedStack<ChangedFeatureStub>& deletedRelations()
    {
        return deletedFeatures(FeatureType::RELATION);
    }

    // TODO: Rename! It's not clear that this is only
    //  for ways and relations
    //  Better yet, unify the stacks
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

    /// Indicates that the given feature may gain or lose a TEX
    /// If feature is non-null, it will have a TEX in the future
    /// (If it doesn't have one already, a new TEX will be assigned
    /// during TEX resolution).
    /// If feature is nullptr, it will not have a TEX (if it had
    /// a TEX, its TEX slot will be cleared during TEX resolution).
    ///
    void texChange(int32_t handle, CFeature* feature)
    {
        if (handle == 0)
        {
            // For new feature, we can't use the hashmap
            // (It doesn't have a handle; always 0), so we
            // stash it; TODO: THis is hacky
            exportTableChanges_.emplace_back(Tex(), false, feature);
            return;
        }
        texChanges_[handle] = feature;
    }

    void resolveExports(TilePtr pTile);
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
    LinkedStack<ChangedFeatureStub> deleted_[3];
    Tip tip_;
    Tile tile_;
    TilePtr tilePtr_;

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
