// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "ChangedFeatureBase.h"
#include <geodesk/feature/WayPtr.h>

class ChangedFeature2D : public ChangedFeatureBase
{
public:
    ChangedFeature2D(FeatureType type, uint64_t id) :
        ChangedFeatureBase(type, id),
        memberCount_(0),
        removedRefcyleCount_(0),
        members_(nullptr)
    {
    }

    ChangedFeature2D* next() const noexcept { return cast(next_); }

    static ChangedFeature2D* cast(CFeatureStub* f)
    {
        assert(!f || f->isChanged());
        assert(!f || f->type() != FeatureType::NODE);
        return reinterpret_cast<ChangedFeature2D*>(f);
    }

    static const ChangedFeature2D* cast(const CFeatureStub* f)
    {
        assert(!f || f->isChanged());
        assert(!f || f->type() != FeatureType::NODE);
        return reinterpret_cast<const ChangedFeature2D*>(f);
    }

    FeaturePtr getFeature(FeatureStore* store) const
    {
        FeaturePtr feature = ref_.getFeature(store);
        if(feature.isNull())    [[unlikely]]
        {
            feature = refSE_.getFeature(store);
        }
        return feature;
    }

    int memberCount() const noexcept { return memberCount_; }

    static size_t size(FeatureType type, size_t memberCount) noexcept
    {
        return sizeof(ChangedFeature2D) + (memberCount-1) * sizeof(CFeature*) +
            (type == FeatureType::RELATION ? (memberCount * sizeof(Role)) : 0);
            // Note: We don't subtract 1 from memberCount to get the size of the role table,
            // since we don't have an entry already in this class
    }

    std::span<CFeatureStub*> members() const { return std::span(members_, memberCount_); }
    void setMembers(std::span<CFeatureStub*> members)
    {
        members_ = members.data();
        memberCount_ = static_cast<int>(members.size());
    }

    std::span<Role> roles() const
    {
        assert(type() == FeatureType::RELATION);
        Role* roles = reinterpret_cast<Role*>(members_ + memberCount_);
        return std::span(roles, memberCount_);
    }

    void compareWayMembers(FeatureStore* store, WayPtr pastWay);

    const Box& bounds() const { return bounds_; }

    /// Sets the bounds of the changed feature, but does not mark
    /// BOUNDS_CHANGED (This allows the feature's past bounds to be
    /// recorded for later comparison).
    ///
    void setBounds(const Box& bounds)
    {
        assert(!bounds.isEmpty());
        assert(!(bounds.bottomLeft().isNull() && bounds.topRight().isNull()));
        bounds_ = bounds;
        // don't set BOUNDS_CHANGED
    }

protected:
    Box bounds_;
    int memberCount_;
    int removedRefcyleCount_;
    CFeatureStub** members_;
};
