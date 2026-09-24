// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangedTile.h"

#include <geodesk/geom/index/HilbertDistanceInTile.h>
#include "ChangedFeature2D.h"
#include "geodesk/query/FeatureFinder.h"

// TODO

/*
void ChangedTile::recordTexChange(CFeature* feature, bool willHaveTex)
{
    bool isSE = false;
    CRef ref = feature->ref();
    if (ref.tip() != tip_) [[unlikely]]
    {
        assert(feature->type() != FeatureType::NODE);
        ref = feature->refSE();
        assert(ref.tip() == tip_);
        isSE = true;
    }
    if (ref.isUnresolved()) [[unlikely]]
    {
        FeatureFinder finder;
        finder.find(ref.getFeature())
    }
}
*/

void ChangedTile::resolveExports(TilePtr pTile)
{
    assert(pTile == tilePtr_);
    if (texChanges_.empty() && exportTableChanges_.empty()) return;

    HilbertDistanceInTile hilbert(tile_);
    std::vector<SortedFeature> sorted;

    // TODO: This is hacky, we're stashing new features
    //  (which always need a TEX) in exportTableChanges_,
    //  and process them first; then we clear exportTableChanges_
    //  so the actual changes can be stored there

    for (auto entry : exportTableChanges_)
    {
        CFeature* feature = entry.feature;
        assert(feature->isChanged());
        Coordinate center;
        if (feature->type() == FeatureType::NODE)
        {
            center = feature->xy();
        }
        else
        {
            center = ChangedFeature2D::cast(feature)->bounds().center();
        }
        sorted.emplace_back(hilbert.compute(center), feature);
    }
    exportTableChanges_.clear();

    uint32_t exportsCount = 0;
    bool tableChanged = false;
    ExportTablePtr exports = pTile.exports();
    if (exports)
    {
        exportsCount = exports.count();
        for (uint32_t i =0; i<exportsCount; i++)
        {
            Tex tex(i);
            FeaturePtr exported = exports.featureAt(tex);
            if (exported.isNull())
            {
                // Existing empty slot
                exportTableChanges_.emplace_back(tex, false, nullptr);
            }
            else
            {
                int32_t handle = pTile.handleOf(exported);
                auto it = texChanges_.find(handle);
                if (it != texChanges_.end())
                {
                    CFeatureStub* stub = it->second;
                    if (stub == nullptr)
                    {
                        // Newly created empty slot
                        exportTableChanges_.emplace_back(tex, true, nullptr);
                        tableChanged = true;
                    }
                    else
                    {
                        // Feature doesn't need a TEX, because it
                        // already has one

                        CFeature* feature = stub->get();
                        CRef newRef = CRef::ofExported(tip_, tex);
                        if (feature->ref().tip() == tip_) [[likely]]
                        {
                            feature->setRef(newRef);
                        }
                        else
                        {
                            assert(feature->refSE().tip() == tip_);
                            feature->setRefSE(newRef);
                        }
                    }
                    texChanges_.erase(it);
                }
            }
        }
    }

    // TODO: Clean this up, too complicated as an early-exit condition
    if (!tableChanged && texChanges_.empty() && sorted.empty())
    {
        return;
    }

    // Any remaining features in texChanges_ actually need to have
    // a TEX assigned to them

    // Sort the features needing a new TEX by Hilbert distance within
    // the tile (improves locality within the export table)
    // TODO: We could reuse a common vector for sorting

    for (const auto& [handle, stub] : texChanges_)
    {
        if (stub == nullptr)
        {
            // A null stub at this point means that a feature
            // may have had a TEX that it needed to lose, but it
            // turned up it wasn't exported, after all; ignore it
            continue;
        }
        assert(stub);
        CFeature* feature = stub->get();
        Coordinate center;
        if (feature->type() == FeatureType::NODE)
        {
            center = feature->xy();
        }
        else
        {
            if (feature->isChanged())
            {
                center = ChangedFeature2D::cast(feature)->bounds().center();
            }
            else
            {
                center = pTile.getFeature(handle).bounds().center();
            }
        }
        sorted.emplace_back(hilbert.compute(center), feature);
    }
    texChanges_.clear();   // TODO: Needed?
    std::sort(sorted.begin(), sorted.end());

    // Place the features into exportTableChanges_, filling any
    // existing empty slots first

    size_t emptySlotCount = exportTableChanges_.size();
    size_t pos = 0;
    for (SortedFeature sortedFeature : sorted)
    {
        CFeature* feature = sortedFeature.feature;
        if (feature->typedId() == TypedFeatureId::ofWay(1340662848))
        {
            LOGS << "!!!";
        }

        Tex tex;
        if (pos < emptySlotCount)
        {
            // We can reuse an empty slot
            exportTableChanges_[pos].changed = true;
            exportTableChanges_[pos].feature = feature;
            tex = exportTableChanges_[pos].tex;
        }
        else
        {
            // Otherwise, append to the end of the table
            tex = Tex(exportsCount);
            exportTableChanges_.emplace_back(tex, true, feature);
            exportsCount++;
        }

        // Set the newly-assigned TEX
        CRef ref = CRef::ofExported(tip_, tex);
        if (feature->ref().tip() == tip_) [[likely]]
        {
            feature->setRef(ref);
        }
        else
        {
            assert(feature->refSE().tip() == tip_);
            feature->setRefSE(ref);
        }
        pos++;
    }

    // Now exportTableChanges_ contains all TEX changes, as well as
    // any existing empty slots that haven't been filled
    // (We need those to see how far the updated export table can
    // be trimmed -- or deleted altogether -- since empty slots
    // at the end are not allowed)

    while (!exportTableChanges_.empty())
    {
        auto& entry = exportTableChanges_.back();
        assert(static_cast<uint32_t>(entry.tex) < exportsCount);
        if (entry.feature != nullptr || entry.tex != exportsCount-1)
        {
            break;
        }
        // Remove the empty slot at end of the table
        exportTableChanges_.pop_back();
        exportsCount--;
    }
    futureExportsCount_ = exportsCount;

    // (exportTableChanges_ may still have existing empty slots)

    // TODO: Need to resolve: Encoding if export table trimmed, but no entries changed
}


void ChangedTile::dump() const
{
    LOGS << "ChangedTile " << tip_ << " (" << tile_ << ") at "
        << tilePtr_.ptr() << " -- this = " << this << ":";
    LOGS << "  " << exportTableChanges_.size() << " new TEXes needed";
    LOGS << "  " << texChanges_.size() << " potential TEX changes";
}