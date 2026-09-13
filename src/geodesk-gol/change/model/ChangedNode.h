// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "ChangedFeatureBase.h"

class ChangedNode : public ChangedFeatureBase
{
public:
    explicit ChangedNode(uint64_t id) :
        ChangedFeatureBase(FeatureType::NODE, id)
    {
    }

    ChangedNode* next() const noexcept { return cast(next_); }

    NodePtr getFeature(FeatureStore* store) const
    {
        return NodePtr(ref_.getFeature(store));
    }

    static ChangedNode* cast(CFeatureStub* f)
    {
        assert(!f || f->isChanged());
        assert(!f || f->type() == FeatureType::NODE);
        return reinterpret_cast<ChangedNode*>(f);
    }

    static const ChangedNode* cast(const CFeatureStub* f)
    {
        assert(!f || f->isChanged());
        assert(!f || f->type() == FeatureType::NODE);
        return reinterpret_cast<const ChangedNode*>(f);
    }
};

