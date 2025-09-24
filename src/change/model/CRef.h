// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <geodesk/feature/FeaturePtr.h>
#include <geodesk/feature/FeatureStore.h>
#include <geodesk/feature/ForeignFeatureRef.h>
#include <geodesk/feature/TileConstants.h>
#include <geodesk/feature/Tip.h>
#include <geodesk/feature/Tex.h>

using namespace geodesk;

// TODO: Rename, same class in compile/

// TODO: Need multiple states:
//  - unknown
//  - verified missing
//  - TIP & offset
//  - TIP & TEX
//  - Anonymous node
//  - single-tile way/relation
//  - TIP (new feature in this tile)

// TODO: encode 4 states:
// - not exported, maybe exported, exported and special

// TODO: Should NW/SE encoded in the CRef?
//  Possible needed for ChangeAction subtypes

// TODO: make state values powers of 2, so multiple values can be checked via
//  bitmask? needed for offerRef(), which must not overwrite NEW/MISSING
//  isReplaceable if none of these bits are set:
//   Bit 1 (export status is definite: status category 2 or 3)
//   Bit ? missing (status category 0)
//   Bit ? new (status category 0)

/// A reference to an existing feature tracked by the ChangeModel.
/// A CRef only tracks a feature in a single tile. For dual-tile features,
/// two CRef instances are needed.
///
/// This class consists of a 64-bit value of the following format:
///     Bit 0-1
///       0 = special (see codes below)
///       1 = possibly_exported
///       2 = not_exported
///       3 = definitely_exported
///     Bit 2-31
///       If special:
///         0 = unknown
///         1 = unresolved
///         2 = missing
///         3 = single-tile feature
///         4 = anonymous node
///         5 = new
///       If possibly_exported or not_exported:
///         offset of existing feature (from tile pointer)
///       If definitely_exported:
///         TEX of existing feature
///     Bit 32-63
///       If new: future TIP
///       If possibly_exported, not_exported, definitely_exported or
///         unresolved: TIP of exiting feature
///       Else: 0
///
/// Important: This class must always be 8-byte aligned, to allow
/// atomic updates by TileChangeAnalyzer, without expensive locking
///
/// A reference can be one of the following:
/// - An existing feature that is not exported (TIP and offset)
/// - An existing feature that is exported (TIP and TEX)
/// - An existing feature that *may be* exported (TIP and offset)
/// - ANONYMOUS_NODE: An anonymous node (TIP is null)
/// - UNKNOWN: Feature may exist, hasn't been found yet (TIP is null)
/// - SINGLE_TILE: Indicates that the feature does not have a twin
///   (TIP is null); can only be used as a SE ref, never as a NW ref)
/// - MISSING: Feature does not exist (TIP is null)
/// - NEW: Feature has been created, or moved to a new tile
///   (TIP of future tile)
/// - UNRESOLVED: Feature exists, but hasn't been located (TIP only);
///   this can happen if one twin of a dual-tile feature has been
///   located -- from its bbox we can determine the tile of its twin
///
/// Observations:
/// - If TIP is non-null, the feature exists (in past and/or future).
/// - If the Ref's state is not "special", its past feature can be retrieved
///
class CRef
{
    enum StatusCategory
    {
        SPECIAL = 0,
        MAYBE_EXPORTED = 1,
        NOT_EXPORTED = 2,
        EXPORTED = 3
    };

    // All of these values must leave Bit 0 and 1 (status category) as 0
    // Even though these are not flags, we make them powers-of-2 to allow
    // us to quickly check for multiple possible values via a bit mask
    enum SpecialStatus
    {
        SPECIAL_UNKNOWN = 0,
        SPECIAL_UNRESOLVED = 1 << 2,
        SPECIAL_MISSING = 1 << 3,
        SPECIAL_SINGLE_TILE = 1 << 4,
        SPECIAL_ANONYMOUS_NODE = 1 << 5,
        SPECIAL_NEW = 1 << 6
    };

public:
    constexpr CRef() : data_(SPECIAL_UNKNOWN) {}

    static constexpr CRef ofExported(Tip tip, Tex tex)
    {
        return CRef((static_cast<uint64_t>(tip) << 32) |
            (static_cast<int>(tex) << 2) | EXPORTED);
    }

    static constexpr CRef ofForeign(ForeignFeatureRef foreign)
    {
        // assert(!foreign.isNull());
        return ofExported(foreign.tip, foreign.tex);
    }

    static constexpr CRef ofNew(Tip tip)
    {
        return CRef((static_cast<uint64_t>(tip) << 32) | SPECIAL_NEW);
    }

    static constexpr CRef ofUnresolved(Tip tip)
    {
        return CRef((static_cast<uint64_t>(tip) << 32) | SPECIAL_UNRESOLVED);
    }

    static constexpr CRef ofNotExported(Tip tip, int32_t handle)
    {
        return CRef((static_cast<uint64_t>(tip) << 32) |
            (handle << 2) | NOT_EXPORTED);
    }

    static constexpr CRef ofMaybeExported(Tip tip, int32_t handle)
    {
        return CRef((static_cast<uint64_t>(tip) << 32) |
            (handle << 2) | MAYBE_EXPORTED);
    }

    constexpr Tip tip() const { return Tip(static_cast<uint32_t>(data_ >> 32)); }

    uint32_t offset() const
    {
        assert(statusCategory() == NOT_EXPORTED ||
            statusCategory() == MAYBE_EXPORTED);
        return static_cast<uint32_t>(data_) >> 2;
    }

    Tex tex() const
    {
        assert(statusCategory() == EXPORTED);
        return Tex(static_cast<uint32_t>(data_) >> 2);
    }

    // TODO: Clarify: UNRESOLVED may also have a TEX!
    bool mayHaveTex() const
    {
        return data_ & 1;
            // The lowest bit is set if the ref is exported or
            // maybe_exported, which means this ref *may* have a TEX
    }

    bool isExported() const
    {
        return statusCategory() == EXPORTED;
    }

    bool isNew() const
    {
        return static_cast<uint32_t>(data_) == SPECIAL_NEW;
    }

    /*
     // TODO: Would have to reorder values: UNKNOWN = 0, MISSING = 1
    bool isUnknownOrMissing() const
    {
        return (static_cast<uint32_t>(data_) & 0xffff'fffb) == 0;
        // All bits except #2 must be clear
    }
    */

    FeaturePtr getFeature(DataPtr pTile) const
    {
        uint32_t stat = statusCategory();
        if(stat == EXPORTED)
        {
            DataPtr pExports = (pTile + TileConstants::EXPORTS_OFS).follow();
            return FeaturePtr((pExports + static_cast<int>(tex()) * 4).follow());
        }
        assert(stat == NOT_EXPORTED || stat == MAYBE_EXPORTED);
        return FeaturePtr(pTile + offset());
    }

    bool canGetFeature() const
    {
        return statusCategory() != SPECIAL;
    }

    FeaturePtr getFeature(FeatureStore* store) const
    {
        if (!canGetFeature()) return FeaturePtr();
        assert(!tip().isNull());
        DataPtr pTile = store->fetchTile(tip());
        return getFeature(pTile);
    }

    /// Checks if this ref can be replaced by a "better" ref.
    /// A ref is "better" if it replaces a MAYBE_EXPORTED ref,
    /// or an unknown or unresolved ref.
    ///
    /// Note: We never replace MISSING or NEW, to avoid clobbering
    /// a computed tile of a deleted or changed feature.
    ///
    bool isVague() const
    {
        uint32_t status = static_cast<uint32_t>(data_);
        return (status & (2 | SPECIAL_MISSING | SPECIAL_NEW)) == 0;
            // Bit 1 (=2) is set for EXPORTED or NOT_EXPORTED refs,
            // but never for MAYBE_EXPORTED or special refs
            // Special refs apart from MISSING or NEW can be replaced
    }

    bool isUnknownOrMissing() const
    {
        return (data_ & (0xffff'ffff & ~SPECIAL_MISSING)) == 0;
    }

    constexpr bool operator==(const CRef& other) const noexcept = default;
    constexpr bool operator!=(const CRef& other) const noexcept = default;

    static const CRef UNKNOWN;
    static const CRef MISSING;
    static const CRef SINGLE_TILE;
    static const CRef ANONYMOUS_NODE;

    template<typename Stream>
    void writeTo(Stream& out)
    {
        switch(statusCategory())
        {
        case SPECIAL:
            switch(static_cast<uint32_t>(data_))
            {
            case SPECIAL_UNKNOWN:
                out << "unknown";
                return;
            case SPECIAL_UNRESOLVED:
                out << "unresolved " << tip();
                return;
            case SPECIAL_MISSING:
                out << "missing";
                return;
            case SPECIAL_SINGLE_TILE:
                out << "single_tile";
                return;
            case SPECIAL_ANONYMOUS_NODE:
                out << "anon_node";
                return;
            case SPECIAL_NEW:
                out << "new " << tip();
                return;
            default:
                out << "!!!invalid_ref!!!";
                return;
            }
        case MAYBE_EXPORTED:
            out << "maybe_exported " << tip() << " @" << offset();
            return;
        case NOT_EXPORTED:
            out << "not_exported " << tip() << " @" << offset();
            return;
        case EXPORTED:
            out << "exported " << tip() << " #" << static_cast<int>(tex());
            return;
        default:
            out << "!!!invalid_ref!!!";
        }
    }

private:
    constexpr explicit CRef(uint64_t data) : data_(data) {}

    uint32_t statusCategory() const { return static_cast<uint32_t>(data_ & 3); }

    uint64_t data_;
};

inline constexpr CRef CRef::UNKNOWN{SPECIAL_UNKNOWN};
inline constexpr CRef CRef::MISSING{SPECIAL_MISSING};
inline constexpr CRef CRef::SINGLE_TILE{SPECIAL_SINGLE_TILE};
inline constexpr CRef CRef::ANONYMOUS_NODE{SPECIAL_ANONYMOUS_NODE};

template<typename Stream>
Stream& operator<<(Stream& out, CRef ref)
{
    ref.writeTo(out);
    return static_cast<Stream&>(out);
}

