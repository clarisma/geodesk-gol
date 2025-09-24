// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "CRef.h"

class CFeature;
class ChangedFeatureBase;

class CFeatureStub
{
public:
    // TODO: more natural to make the flag mean *local*, so 0 can be
    //  an empty role (global string 0)?
    //  No, must keep flag to comply with TES spec, which uses flag=1
    //  to indicate global
    class Role
    {
    public:
        Role() : data_(1) {}
        Role(bool global, uint32_t value) :
            data_((value << 1) | global) {}

        bool isGlobal() const { return data_ & 1; }
        bool isGlobal(int code) const
        {
            return data_ == ((code << 1) | 1);
        }
        uint32_t value() const { return data_ >> 1; }

        bool operator==(const Role other) const
        {
            return data_ == other.data_;
        }

        bool operator!=(const Role other) const
        {
            return data_ != other.data_;
        }

        explicit operator uint32_t() const
        {
            return data_;
        }

    private:
        uint32_t data_;
    };

    CFeatureStub() : idAndFlags_(0), ref_{}, refSE_{} {}

    CFeatureStub(int flags, FeatureType type, uint64_t id) :
        idAndFlags_((id << (FLAG_COUNT + 2)) |
            (static_cast<int>(type) << FLAG_COUNT) | flags),
        ref_{},
        refSE_{}
    {
    }

    TypedFeatureId typedId() const noexcept
    {
        return TypedFeatureId(idAndFlags_ >> FLAG_COUNT);
    }

    uint64_t id() const noexcept
    {
        return idAndFlags_ >> (FLAG_COUNT + 2);
    }

    FeatureType type() const noexcept
    {
        return static_cast<FeatureType>((idAndFlags_ >> FLAG_COUNT) & 3);
    }

    bool isChanged() const { return idAndFlags_ & CHANGED; }
    bool isReplaced() const { return idAndFlags_ & REPLACED; }
    bool isBasic() const { return (idAndFlags_ & (CHANGED |REPLACED)) == 0; }

    ChangedFeatureBase* getReplaced() const
    {
        assert(isReplaced());
        return changed_;
    }

    CFeature* get()
    {
        if(isReplaced())  [[unlikely]]
        {
            // We need these casts because ChangedFeatureBase has not
            // been declared yet

            CFeature* changed = reinterpret_cast<CFeature*>(changed_);
            assert(!reinterpret_cast<CFeatureStub*>(changed)->isReplaced());
            return changed;
        }
        return reinterpret_cast<CFeature*>(this);
    }

    const CFeature* get() const
    {
        return const_cast<CFeatureStub*>(this)->get();
    }

    void replaceWith(ChangedFeatureBase* changed)
    {
        idAndFlags_ |= REPLACED;
        changed_ = changed;
        reinterpret_cast<CFeatureStub*>(changed)->idAndFlags_ |=
            idAndFlags_ & (FUTURE_WAYNODE | FUTURE_FOREIGN);
            // The changed feature assumes these flags
    }

protected:
    static constexpr int FLAG_COUNT = 4;
    static constexpr int REPLACED = 1;
    static constexpr int CHANGED = 2;
    static constexpr int FUTURE_WAYNODE = 4;
    static constexpr int FUTURE_FOREIGN = 8;

    uint64_t idAndFlags_;
    union
    {
        ChangedFeatureBase* changed_;
        CRef ref_;
    };
    union
    {
        CRef refSE_;
        Coordinate xy_;
    };
};


// Storage cost
// node (unchanged):         24 bytes
// node (changed):           56 bytes
// way/relation (unchanged): 24 bytes
// way/relation (changed):   88 bytes + members

// Instead of "Memberships", tracked added/removed
