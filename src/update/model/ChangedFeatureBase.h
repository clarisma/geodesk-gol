// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "CFeature.h"
#include "ChangeAction.h"
#include "ChangeFlags.h"
#include <clarisma/util/log.h>
#include <atomic>

class ChangedFeatureBase;
class CRelationTable;
class CTagTable;

class ChangedFeatureStub : public CFeature
{
public:
    using ChangeFlagsBits = std::underlying_type_t<ChangeFlags>;

    explicit ChangedFeatureStub(ChangedFeatureBase* feature) :
        CFeature(CHANGED | REPLACED,
            reinterpret_cast<CFeature*>(feature)->type(),
            reinterpret_cast<CFeature*>(feature)->id())
            // TODO: Are these casts needed?
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
        return test(flags(), ChangeFlags::DELETED);
    }

    bool isChangedExplicitly() const
    {
        // TODO: Are we forcing version to 1 if not specified in .osc?
        return version_ != 0;
    }

    bool hasActualChanges() const
    {
        return (static_cast<ChangeFlags>(flags_) & (
            ChangeFlags::TAGS_CHANGED |
            ChangeFlags::GEOMETRY_CHANGED |
            ChangeFlags::MEMBERS_CHANGED |
            ChangeFlags::WAYNODE_IDS_CHANGED |
            ChangeFlags::BOUNDS_CHANGED |
            ChangeFlags::FLAGS_CHANGED |
            ChangeFlags::ADDED_TO_RELATION |
            ChangeFlags::REMOVED_FROM_RELATION |
            ChangeFlags::RELTABLE_CHANGED))
            != ChangeFlags::NONE;
    }

    ChangeFlags flags() const noexcept
    {
        return static_cast<ChangeFlags>(flags_);
    }

    bool is(ChangeFlags f) const noexcept
    {
        return test(flags(), f);
    }

    bool isAny(ChangeFlags f) const noexcept
    {
        return testAny(flags(), f);
    }

    void setFlags(ChangeFlags flags)
    {
        flags_ = static_cast<uint32_t>(flags);
    }

    void addFlags(ChangeFlags flags)
    {
        flags_ |= static_cast<uint32_t>(flags);
    }

    void clearFlags(ChangeFlags flags)
    {
        flags_ &= ~static_cast<uint32_t>(flags);
    }

    void clearFlagsConcurrent(ChangeFlags flags)
    {
        std::atomic_ref ref(flags_);
        ref.fetch_and(~static_cast<uint32_t>(flags), std::memory_order_relaxed);
    }

    void addMembershipChange(MembershipChange* action)
    {
        assert(!test(flags(), ChangeFlags::RELTABLE_LOADED));
        action->setNext(membershipChanges_);
        membershipChanges_ = action;
    }

    const MembershipChange* membershipChanges() const
    {
        assert(!is(ChangeFlags::RELTABLE_LOADED));
        return membershipChanges_;
    }

    /// Returns the relation table of this changed feature,
    /// which can be `nullptr` if it has never been loaded
    /// (i.e. it does not process or retrieve it)
    ///
    const CRelationTable* peekParentRelations() const
    {
        if (!is(ChangeFlags::RELTABLE_LOADED) &&
            parentRelations_ != nullptr)
        {
            LOGS << typedId()
                << ": Attempt to dereference a reltable which"
                << " has not been processed or retrieved";
        }
        assert(is(ChangeFlags::RELTABLE_LOADED) ||
            parentRelations_ == nullptr);
        return parentRelations_;
    }

    void setParentRelations(const CRelationTable* rels)
    {
        parentRelations_ = rels;
        addFlags(ChangeFlags::RELTABLE_LOADED);
    }

    ChangedFeatureBase* next() const noexcept
    {
        assert(!isReplaced());
        return reinterpret_cast<ChangedFeatureBase*>(next_);
    }

protected:
    ChangedFeatureBase(FeatureType type, uint64_t id) :
        ChangedFeatureStub(type, id),
        flags_(0),
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

    ChangeFlagsBits flags_;  // ChangeFlags
    uint32_t version_;
    const CTagTable* tags_;
    union
    {
        MembershipChange* membershipChanges_;
        const CRelationTable* parentRelations_;
    };
};

