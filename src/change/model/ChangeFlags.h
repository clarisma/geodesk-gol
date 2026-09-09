// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <cstdint>

// TODO: Sort these
//  - general
//  - relation changes
//  - node status


// TODO: Useful to indicate implicit/technical change
//  For explicit changes, we will always get both twins of a dual-tile feature
//   (hence we can turn an unknown SE ref into SINGLE_TILE after the first search is done)
//   (This is because we look up both tiles of explicitly changed ways/relations and
//    add them to the search scope)
//  For implicit/technical, the same guarantee does not apply

// TODO: Missing features added to relation
//  When processing ways, we discover that we don't have any refs
//   Look up in index, find 0, mark as missing
//   Check if feature is new:
//     If not new (truly missing), throw away the change (unless explicitly created new)

enum class ChangeFlags : uint32_t
{
    NONE = 0,
    /// The feature was changed directly by a user:
    /// - Created or deleted
    /// - Changed tags
    /// - If node: changed location
    /// - If way: added, removed or rearranged nodes
    /// - If relation: added, removed or rearranged members
    /// Changes to a way's nodes or a relation's members
    /// is *not* an explciit change to the way/relation
    /// In short, any change reported in an .osc file
    // EXPLICITLY_CHANGED = 1 << 0,
    // CREATED = 1 << 1,
    /// The feature has been deleted (always explicit)
    DELETED = 1 << 2,

    /// The feature was added to at least one relation
    ADDED_TO_RELATION = 1 << 3,

    /// The feature was removed from at least one relation
    REMOVED_FROM_RELATION = 1 << 4,

    /// The feature has been added to or removed from a relation,
    /// or a parent relation moved to a different tile; also set
    /// if a ChangedFeature (or a copy) newly appears in a tile
    ///  TODO: may not be true anymore with intro of ChangedFeatureStubs
    RELTABLE_LOADED = 1 << 5,
        // rename to RELTABLE_CHANGED

    TAGS_CHANGED = 1 << 6,
    GEOMETRY_CHANGED = 1 << 7,
    MEMBERS_CHANGED = 1 << 8,
    WAYNODE_IDS_CHANGED = 1 << 9,
    BOUNDS_CHANGED = 1 << 10,
    TILES_CHANGED = 1 << 11,
    RELTABLE_CHANGED = 1 << 12,
    FLAGS_CHANGED = 1 << 13,

    /// The node will have the same location as another node
    /// (though it may not necessarily be a duplicate)
    FLAGGED_SHARED_LOCATION = 1 << 14,
    FLAGGED_EXCEPTION_NODE = 1 << 15,
    FLAGGED_AREA = 1 << 16,
    FLAGGED_WAYNODE = 1 << 17,

    PROCESSED = 1 << 18,
    REMOVED_FROM_WAY = 1 << 19,

    RELATION_DEFERRED = 1 << 20,
    RELATION_ATTEMPTED = 1 << 21,
    NEW_TO_NORTHWEST = 1 << 22,
    NEW_TO_SOUTHEAST = 1 << 23,
    WAY_WILL_HAVE_FEATURE_NODES = 1 << 24,
    WILL_BE_SUPER_RELATION = 1 << 25
};

// TODO: do we need the NEW_ flags? can derive from ref?

constexpr ChangeFlags operator|(ChangeFlags a, ChangeFlags b) noexcept
{
    return static_cast<ChangeFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr ChangeFlags operator&(ChangeFlags a, ChangeFlags b) noexcept
{
    return static_cast<ChangeFlags>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr ChangeFlags operator~(ChangeFlags a) noexcept
{
    return static_cast<ChangeFlags>(~static_cast<uint32_t>(a));
}

constexpr ChangeFlags& operator|=(ChangeFlags& a, ChangeFlags b) noexcept
{
    return a = a | b;
}

constexpr ChangeFlags& operator&=(ChangeFlags& a, ChangeFlags b) noexcept
{
    return a = a & b;
}

constexpr bool test(ChangeFlags a, ChangeFlags b) noexcept
{
    return (a & b) != ChangeFlags::NONE;
}

constexpr bool testAny(ChangeFlags flag, ChangeFlags multiple) noexcept
{
    // same as test, but name makes it more explicit
    return (flag & multiple) != ChangeFlags::NONE;
}


