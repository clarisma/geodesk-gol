// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangedTile.h"

#include <geodesk/geom/index/HilbertDistanceInTile.h>
#include "ChangedFeature2D.h"

void ChangedTile::resolveExports(TilePtr tilePtr)
{
    if (texChanges_.empty()) return;

    bool tableChanged = false;
    ExportTablePtr exports = tilePtr.exports();
    uint32_t exportsCount = 0;
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
                int32_t handle = tilePtr.handleOf(exported);
                auto it = texChanges_.find(handle);
                if (it != texChanges_.end())
                {
                    CFeatureStub* stub = it->second;
                    if (stub == nullptr)
                    {
                        // Newly created empty slot
                        exportTableChanges_.emplace_back(tex, false, nullptr);
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

    if (!tableChanged)
    {
        assert(texChanges_.empty());
        return;
    }

    // Any remaining features in texChanges_ actually need to have
    // a TEX assigned to them

    // Sort the features needing a new TEX by Hilbert distance within
    // the tile (improves locality within the export table)
    // TODO: We could reuse a common vector for sorting

    HilbertDistanceInTile hilbert(tile_);
    std::vector<SortedFeature> sorted;
    for (const auto& [handle, stub] : texChanges_)
    {
        assert(stub);   // TEX losers should be gone at this point
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
                center = FeaturePtr(tilePtr + handle).bounds().center();
            }
        }
        sorted.emplace_back(hilbert.compute(center), feature);
    }
    std::sort(sorted.begin(), sorted.end());

    // Place the features into exportTableChanges_, filling any
    // existing empty slots first

    size_t pos = 0;
    for (SortedFeature sortedFeature : sorted)
    {
        CFeature* feature = sortedFeature.feature;
        Tex tex;
        if (pos < exportTableChanges_.size())
        {
            // We can reuse an empty slot
            exportTableChanges_[pos].changed = true;
            exportTableChanges_[pos].feature = feature;
            tex = exportTableChanges_[pos].tex;
        }
        else
        {
            // Otherwise, append to the end of the table
            exportTableChanges_.emplace_back(
                Tex(exportsCount), true, feature);
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
