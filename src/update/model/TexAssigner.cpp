// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include "TexAssigner.h"

void TexAssigner::assign(Tip tip, TilePtr tilePtr)
{
    bool tableChanged = false;
    ExportTablePtr exports = tilePtr.exports();
    if (exports)
    {
        uint32_t count = exports.count();
        for (uint32_t i =0; i<count; i++)
        {
            Tex tex(i);
            FeaturePtr feature = exports.featureAt(tex);
            if (feature.isNull())
            {
                // Existing empty slot
                exportTableChanges_.emplace_back(tex, false, nullptr);
            }
            else
            {
                int32_t handle = tilePtr.handleOf(feature);
                auto it = texChanges_.find(handle);
                if (it != texChanges_.end())
                {
                    ChangedFeatureBase* changed = it->second;
                    if (changed == nullptr)
                    {
                        // Newly created empty slot
                        exportTableChanges_.emplace_back(tex, false, nullptr);
                        tableChanged = true;
                    }
                    else
                    {
                        // Feature doesn't need a TEX, because it
                        // already has one

                        CRef newRef = CRef::ofExported(tip, tex);
                        if (changed->ref().tip() == tip) [[likely]]
                        {
                            changed->setRef(newRef);
                        }
                        else
                        {
                            assert(changed->refSE().tip() == tip);
                            changed->setRefSE(newRef);
                        }
                    }
                    texChanges_.erase(it);
                }
            }
        }
    }

    // Any remaining features in texChanges_ actually need to have
    // a TEX assigned to them

    // TODO: sort by hilbert distance
    // TODO: place into exportTableChanges_, filling any holes first
    // TODO: compute new table size (determine if holes at end, newly
    //  added entries)
}