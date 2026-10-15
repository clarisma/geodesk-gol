// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "ChangedFeatureBase.h"
#include <clarisma/cli/ConsoleWriter.h>

class ChangeStatistics
{
public:
    void tally(const ChangedFeatureBase& changed)
    {
        int type = static_cast<int>(changed.type());
        ChangeFlags flags = changed.flags();
        PerType& perType = perType_[type];
        perType.changed++;
        perType.explicitly += changed.isChangedExplicitly();
        perType.tags += test(flags, ChangeFlags::TAGS_CHANGED);
        perType.geometry += test(flags, ChangeFlags::GEOMETRY_CHANGED);
        perType.addedToRelation += test(flags, ChangeFlags::ADDED_TO_RELATION);
        perType.removedFromRelation += test(flags, ChangeFlags::REMOVED_FROM_RELATION);
    }

    void report() const
    {
        clarisma::ConsoleWriter out;
        out.blank();
        constexpr const char* TYPES[3] = {
            "nodes    ",
            "ways     ",
            "relations" };
        for (int type=0; type<3; type++)
        {
            const PerType& perType = perType_[type];
            out << "Changed " << TYPES[type] << ":       " << perType.changed
                << "\n  Explicitly changed:    " << perType.explicitly
                << "\n  Tags changed:          " << perType.tags
                << "\n  Geometry changed:      " << perType.geometry
                << "\n  Added to relation:     " << perType.addedToRelation
                << "\n  Removed from relation: " << perType.removedFromRelation
                << "\n";
        }
    }

private:
    struct PerType
    {
        uint64_t changed = 0;
        uint64_t explicitly = 0;
        uint64_t tags = 0;
        uint64_t geometry = 0;
        uint64_t addedToRelation = 0;
        uint64_t removedFromRelation = 0;
    };
    PerType perType_[3];

};