// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <geodesk/feature/FeatureStore.h>
#include <geodesk/feature/FeatureTypes.h>
#include <geodesk/format/KeySchema.h>
#include <geodesk/geom/Box.h>
#include "CompleteRelationSet.h"

namespace geodesk {
class Filter;
class MatcherHolder;
}

using namespace geodesk;

class QuerySpec
{
public:
    QuerySpec(FeatureStore* store,
        const Box& box, FeatureTypes types, const MatcherHolder* matcher,
        const Filter* filter, int precision, std::string_view keys) :
        store_(store),
        box_(box),
        matcher_(matcher),
        filter_(filter),
        types_(types),
        precision_(precision),
        keys_(&store->strings(), keys)
    {
    }

    FeatureStore* store() const noexcept { return store_; }
    const Box& box() const noexcept { return box_; }
    const MatcherHolder* matcher() const noexcept { return matcher_; }
    const Filter* filter() const noexcept { return filter_; }
    FeatureTypes types() const noexcept { return types_; }
    int precision() const noexcept { return precision_; }
    const KeySchema& keys() const noexcept { return keys_; }
    void setCompleteRelations(std::string_view list)
    {
        if (list.empty()) list = "multipolygon";
        completeRelations_.update(list, store_->strings());
    }
    bool completeRelation(RelationPtr rel) const noexcept
    {
        return completeRelations_.contains(rel);
    }

private:
    FeatureStore* store_;
    Box box_;
    const MatcherHolder* matcher_;
    const Filter* filter_;
    FeatureTypes types_;
    int precision_;
    const KeySchema keys_;
    CompleteRelationSet completeRelations_;
};

