// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/util/log.h>
#include <geodesk/feature/NodePtr.h>
#include "CFeature.h"
#include "ChangeAction.h"
#include "ChangeFlags.h"
#include "CRef.h"

class ChangedFeatureBase;
class CRelationTable;
class CTagTable;

class ChangedFeatureStub : public CFeature
{
public:
    explicit ChangedFeatureStub(ChangedFeatureBase* feature) :
        CFeature(CHANGED | REPLACED,
            reinterpret_cast<CFeature*>(feature)->type(),
            reinterpret_cast<CFeature*>(feature)->id())
    {
        changed_ = feature;
    }

    ChangedFeatureStub* next() const noexcept { return next_; }
    void setNext(ChangedFeatureStub* next) noexcept { next_ = next; }
    ChangedFeatureBase* get()
    {
        assert(isChanged());
        return reinterpret_cast<ChangedFeatureBase*>(CFeatureStub::get());
    }
    const ChangedFeatureBase* get() const
    {
        assert(isChanged());
        return reinterpret_cast<const ChangedFeatureBase*>(CFeatureStub::get());
    }

protected:
    ChangedFeatureStub(FeatureType type, uint64_t id) :
        CFeature(CHANGED, type, id)
    {
    }

    ChangedFeatureStub* next_ = nullptr;
};


class ChangedFeatureBase : public ChangedFeatureStub
{
public:
    static ChangedFeatureBase* cast(CFeature* f)
    {
        assert(!f || f->isChanged());
        return reinterpret_cast<ChangedFeatureBase*>(f);
    }

    static const ChangedFeatureBase* cast(const CFeature* f)
    {
        assert(!f || f->isChanged());
        return reinterpret_cast<const ChangedFeatureBase*>(f);
    }

    uint32_t version() const { return version_; }
    void setVersion(uint32_t version)
    {
        assert(version >= version_);
        version_ = version;
    }

    const CTagTable* tagTable() const { return tags_; }
    void setTagTable(const CTagTable* tags)
    {
        tags_ = tags;
        // flags_ |= ChangeFlags::TAGS_CHANGED;
        // Don't set flag!
    }

    bool isDeleted() const
    {
        return (flags_ & ChangeFlags::DELETED) != ChangeFlags::NONE;
    }

    bool isChangedExplicitly() const
    {
        return version_ != 0;
    }

    bool hasActualChanges() const
    {
        return (flags_ & (
            ChangeFlags::TAGS_CHANGED |
            ChangeFlags::GEOMETRY_CHANGED |
            ChangeFlags::WAYNODE_IDS_CHANGED |
            ChangeFlags::AREA_STATUS_CHANGED |
            ChangeFlags::BOUNDS_CHANGED |
            ChangeFlags::ADDED_TO_RELATION |
            ChangeFlags::RELTABLE_CHANGED |
            ChangeFlags::REMOVED_FROM_RELATION |
            ChangeFlags::WAYNODE_STATUS_CHANGED |
            ChangeFlags::SHARED_LOCATION_STATUS_CHANGED))
            != ChangeFlags::NONE;
    }

    ChangeFlags flags() const noexcept { return flags_; }

    bool is(ChangeFlags flags) const noexcept
    {
        return test(flags_, flags);
    }

    bool isAny(ChangeFlags flags) const noexcept
    {
        return testAny(flags_, flags);
    }

    void setFlags(ChangeFlags flags)
    {
        flags_ = flags;
    }

    void addFlags(ChangeFlags flags)
    {
        flags_ |= flags;
    }

    void clearFlags(ChangeFlags flags)
    {
        flags_ &= ~flags;
    }

    void addMembershipChange(MembershipChange* action)
    {
        assert(!test(flags_, ChangeFlags::RELTABLE_LOADED));
        action->setNext(membershipChanges_);
        membershipChanges_ = action;
    }

    const MembershipChange* membershipChanges() const
    {
        assert(!test(flags_, ChangeFlags::RELTABLE_LOADED));
        return membershipChanges_;
    }

    const CRelationTable* parentRelations() const
    {
        if (!test(flags_, ChangeFlags::RELTABLE_LOADED) &&
            parentRelations_ != nullptr)
        {
            LOGS << typedId()
                << ": Attempt to dereference a reltable which"
                << " has not been processed or retrieved";
        }
        assert(test(flags_, ChangeFlags::RELTABLE_LOADED) ||
            parentRelations_ == nullptr);
        return parentRelations_;
    }

    void setParentRelations(const CRelationTable* rels)
    {
        parentRelations_ = rels;
        flags_ |= ChangeFlags::RELTABLE_LOADED;
    }

    ChangedFeatureBase* next() const noexcept
    {
        assert(!isReplaced());
        return reinterpret_cast<ChangedFeatureBase*>(next_);
    }

protected:
    ChangedFeatureBase(FeatureType type, uint64_t id) :
        ChangedFeatureStub(type, id),
        flags_(ChangeFlags::NONE),
        version_(0),
        tags_(nullptr),
        membershipChanges_(nullptr)
    {
    }

    /*
    ChangedFeatureBase(FeatureType type, uint64_t id, ChangeFlags flags,
        uint32_t version) :
        ChangedFeatureStub(type, id),
        flags_(flags),
        version_(version),
        tags_(nullptr),
        membershipChanges_(nullptr)
    {
    }
    */

    ChangeFlags flags_;
    uint32_t version_;
    const CTagTable* tags_;
    union
    {
        MembershipChange* membershipChanges_;
        const CRelationTable* parentRelations_;
    };
};

