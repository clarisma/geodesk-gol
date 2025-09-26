// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileChecker.h"

#include <clarisma/cli/ConsoleWriter.h>
#include <geodesk/feature/GlobalStrings.h>
#include <geodesk/feature/NodePtr.h>
#include <geodesk/feature/RelationPtr.h>
#include <geodesk/feature/WayPtr.h>

#include "clarisma/util/log.h"

TileChecker::TileChecker(Tip tip, Tile tile, TilePtr pTile) :
    BinaryChecker(pTile.ptr(), pTile.totalSize()),
    tip_(tip),
    tile_(tile)
{
}


bool TileChecker::check()
{
    tileBounds_ = tile_.bounds();
    checkNodeIndex(start() + NODE_INDEX_OFS);
    checkIndex(start() + WAY_INDEX_OFS, FeatureTypes::NONAREA_WAYS);
    checkIndex(start() + AREA_INDEX_OFS, FeatureTypes::AREAS);
    checkIndex(start() + RELATION_INDEX_OFS, FeatureTypes::NONAREA_RELATIONS);
    checkExports(start() + EXPORTS_OFS);
    for (const Error& error : errors())
    {
        ConsoleWriter out;
        out.blank() << error.location() << ": " << error.message();
    }
    return true;
}

bool TileChecker::checkPointer(DataPtr pBase, int delta)
{
    if (delta == 0)
    {
        error(pBase, "Pointer with 0-offset");
        return false;
    }
    const uint8_t *p = pBase + delta;
    if (p < start() || p >= end())
    {
        error(pBase, "Pointer out of range");
        return false;
    }
    return true;
}


bool TileChecker::checkAccess(DataPtr p, const char* what)
{
    if (p.ptr() < start() || p.ptr() >= end())
    {
        error(p, "%s truncated", what);
        return false;
    }
    return true;
}

void TileChecker::checkNodeIndex(DataPtr ppIndex)
{
    Box bounds;
    int32_t rel = ppIndex.getInt();
    if (rel == 0) return;
    if (rel & 3)
    {
        error(ppIndex, "Invalid node-index pointer");
        return;
    }
    if (!checkPointer(ppIndex, rel)) return;
    DataPtr p = ppIndex + rel;
    for (;;)
    {
        if (!checkAccess(p + 7, "node index")) return;
        int rel = p.getInt();
        int lastFlag = rel & 1;
        rel &= ~1;
        if (rel & 2)
        {
            error(ppIndex, "Invalid node-index pointer");
        }
        else if (checkPointer(p, rel))
        {
            uint32_t keys = (p+4).getUnsignedInt();
            checkNodeTrunk(p+rel, keys, bounds);
                // bounds are not checked
            if (lastFlag != 0) break;
            p += 8;
        }
    }
}


// NOLINTNEXTLINE recursive
uint32_t TileChecker::checkNodeTrunk(DataPtr p, uint32_t keys, Box& actualBounds)
{
    uint32_t actualKeys = 0;
    for (;;)
    {
        if (!checkAccess(p + 19, "index branch")) return INVALID_INDEX;
        int32_t rel = p.getInt();
        int lastFlag = rel & 1;
        int leafFlag = rel & 2;
        rel &= ~3;
        if (checkPointer(p, rel))
        {
            Box actualChildBounds;
            if (leafFlag)
            {
                actualKeys |= checkNodeLeaf(p + rel, keys, actualChildBounds);
            }
            else
            {
                actualKeys |= checkNodeTrunk(p + rel, keys, actualChildBounds);
            }
            if (checkBounds(p.ptr() + 4, actualChildBounds))
            {
                actualBounds.expandToIncludeSimple(actualChildBounds);
            }
        }
        else
        {
            actualKeys = INVALID_INDEX;
        }
        if (lastFlag) break;
        p += 20;
    }
    return actualKeys;
}

uint32_t TileChecker::checkNodeLeaf(DataPtr p, uint32_t keys, Box& actualBounds)
{
    uint32_t actualKeys = 0;
    for (;;)
    {
        if (!checkAccess(p + 19, "node index branch")) return INVALID_INDEX;
        int flags = (p+8).getInt();
        if (!checkAccess(p + 19 + (flags & 4),
            "node index branch"))
        {
            return INVALID_INDEX;
        }
        actualKeys |= checkNode(p+8, actualBounds);
        if (flags & 1) break;
        p += 20 + (flags & 4);
            // If Node is member of relation (flag bit 2), add
            // extra 4 bytes for the relation table pointer
    }
    return actualKeys;
}


void TileChecker::checkIndex(DataPtr ppIndex, FeatureTypes types)
{
    Box bounds;
    int32_t rel = ppIndex.getInt();
    if (rel == 0) return;
    if (rel & 3)
    {
        error(ppIndex, "Invalid index pointer");
        return;
    }
    if (!checkPointer(ppIndex, rel)) return;
    DataPtr p = ppIndex + rel;
    for (;;)
    {
        if (!checkAccess(p + 7, "index")) return;
        rel = p.getInt();
        int lastFlag = rel & 1;
        rel &= ~1;
        if (rel & 2)
        {
            error(ppIndex, "Invalid index pointer");
        }
        else if (checkPointer(p, rel))
        {
            uint32_t keys = (p+4).getUnsignedInt();
            checkTrunk(p+rel, types, keys, bounds);
                // bounds are not checked
            if (lastFlag != 0) break;
            p += 8;
        }
    }
}


// NOLINTNEXTLINE recursive
uint32_t TileChecker::checkTrunk(DataPtr p, FeatureTypes types, uint32_t keys, Box& actualBounds)
{
    uint32_t actualKeys = 0;
    for (;;)
    {
        if (!checkAccess(p + 19, "index branch")) return INVALID_INDEX;
        int32_t rel = p.getInt();
        int lastFlag = rel & 1;
        int leafFlag = rel & 2;
        rel &= ~3;
        if (checkPointer(p, rel))
        {
            Box actualChildBounds;
            if (leafFlag)
            {
                actualKeys |= checkLeaf(p + rel, types, keys, actualChildBounds);
            }
            else
            {
                actualKeys |= checkTrunk(p + rel, types, keys, actualChildBounds);
            }
            if (checkBounds(p.ptr() + 4, actualChildBounds))
            {
                actualBounds.expandToIncludeSimple(actualChildBounds);
            }
        }
        else
        {
            actualKeys = INVALID_INDEX;
        }
        if (lastFlag) break;
        p += 20;
    }
    return actualKeys;
}

uint32_t TileChecker::checkLeaf(DataPtr p, FeatureTypes types, uint32_t keys, Box& actualBounds)
{
    uint32_t actualKeys = 0;
    for (;;)
    {
        if (!checkAccess(p + 31, "index branch")) return INVALID_INDEX;
        FeaturePtr feature(p + 16);
        int flags = feature.flags();
        if (!types.acceptFlags(flags))
        {
            error(p, "Wrong feature type");
            return INVALID_INDEX;
        }
        actualBounds.expandToIncludeSimple(feature.bounds());
        if (feature.isWay())    [[likely]]
        {
            actualKeys |= checkWay(p+16);
        }
        else
        {
            assert(feature.isRelation());
            actualKeys |= checkRelation(p+16);
        }
        if (flags & 1) break;
        p += 32;
    }
    return actualKeys;
}


bool TileChecker::checkBounds(DataPtr pStored)
{
    const Box& stored = *reinterpret_cast<const Box*>(pStored.ptr());
    if (!stored.isSimple() || stored.isEmpty())
    {
        error(pStored, "Invalid bounds");
        return false;
    }
    return true;
}

bool TileChecker::checkBounds(DataPtr pStored, const Box& actual)
{
    if (!checkBounds(pStored)) return false;
    const Box& stored = *reinterpret_cast<const Box*>(pStored.ptr());
    if (stored != actual)
    {
        error(pStored, "Invalid bounds");
        return false;
    }
    return true;
}


bool TileChecker::checkId(FeaturePtr feature)
{
    uint64_t id = feature.id();
    if (id == 0)
    {
        error(feature.ptr(), "Feature with zero-ID");
        return false;
    }
    if (id > 50'000'000'000ULL)
    {
        warning(feature.ptr(), "Suspiciously high feature ID");
    }

    auto [it, inserted] = features_.insert(feature.typedId());
    if (!inserted)
    {
        error("Duplicate feature: %s/%ull", feature.typeName(), feature.id());
        return false;
    }
    return true;
}

uint32_t TileChecker::checkNode(DataPtr p, Box& actualLeafBounds)
{
    FeaturePtr feature(p);
    if (!feature.isNode())
    {
        error(p, "Wrong feature type");
        return INVALID_INDEX;
    }
    checkId(feature);
    int flags = p.getInt();
    if (flags & FeatureFlags::AREA)
    {
        error(p, "Node has area_flag set");
    }
    NodePtr node(feature);
    if (!tileBounds_.contains(node.xy()))
    {
        error(p-8, "Node lies outside of tile bounds");
    }
    else
    {
        actualLeafBounds.expandToInclude(node.xy());
    }

    TagTableInfo tags = checkTagTablePtr(p + 8, feature.typedId());
    if (tags.flags & TagTableInfo::TAGGED_DUPLICATE)
    {

    }
    if (tags.flags & TagTableInfo::TAGGED_ORPHAN)
    {
        if (flags & FeatureFlags::RELATION_MEMBER)
        {
            error(p, "'Orphan' node is a relation member");
        }
    }
    return tags.keys;
}


TileChecker::TagTableInfo TileChecker::checkTagTablePtr(DataPtr ppTags, TypedFeatureId typedId)
{
    int rel = ppTags.getInt();
    int localFlag = rel & 1;
    rel ^= localFlag;
    if (!checkPointer(ppTags, rel)) return TagTableInfo();
    DataPtr pTags = ppTags + rel;
    auto it = tagTables_.find(pTags);
    if (it != tagTables_.end()) return it->second;
    TagTableInfo info = checkTagTable(ppTags + rel, localFlag, typedId);
    tagTables_[pTags] = info;
    return info;
}

TileChecker::TagTableInfo TileChecker::checkTagTable(DataPtr pTags, bool hasLocalTags, TypedFeatureId typedId)
{
    int tagCount = 0;
    TagTableInfo info;
    DataPtr p = pTags;
    int prevGlobalKey = 0;
    for (;;)
    {
        if (!checkAccess(p+3, "tag table")) return info;
        int keyBits = p.getUnsignedShort();
        int type = keyBits & 3;
        int key = (keyBits >> 2) & 0x1fff;
        int lastFlag = keyBits & 0x8000;
        if (key == 0)
        {
            if (!lastFlag || type != TagValueType::GLOBAL_STRING ||
                (p+2).getUnsignedShort() != 0)
            {
                LOGS << typedId << ": Tag table with " << tagCount << " tags, " <<
                    (hasLocalTags ? " has local keys" : " global keys only");
                error(p, "Invalid empty-table tag: %02X %02X %02X %02X",
                    p.getByte(), (p+1).getByte(), (p+2).getByte(), (p+3).getByte());
            }
            else if (prevGlobalKey)
            {
                error(p, "Found empty-table entry, but tag table has global tags");
            }
            break;
        }

        // TODO: check key range
        if (key == prevGlobalKey)
        {
            error(p, "Duplicate global key");
        }
        else if(key < prevGlobalKey)
        {
            error(p, "Wrong order of global keys");
        }

        p += 2;
        checkTagValue(p, type);
        prevGlobalKey = key;
        p += 2 + (type & 2);
        tagCount++;
        if (lastFlag) break;
    }

    if (hasLocalTags)
    {
        // TODO: check locals
        DataPtr pBase = pTags & ~3;
        p = pTags;
        for(;;)
        {
            p -=4;
            if (!checkAccess(p, "tag table")) return info;
            int keyBits = p.getInt();
            int lastFlag = keyBits & 4;
            int type = keyBits & 3;
            if (!checkAccess(p - 2 - (keyBits & 2), "tag table"))
            {
                return info;
            }
            DataPtr pKey = pBase + ((keyBits >> 1) & ~3);
            int rel = pKey - p;
            if (checkPointer(p, rel))
            {
                const ShortVarString* key = checkString(pKey);
                if (*key == "geodesk:duplicate")
                {
                    info.flags |= TagTableInfo::TAGGED_DUPLICATE;
                    if (type != TagValueType::GLOBAL_STRING ||
                        (p-2).getUnsignedShort() != GlobalStrings::YES)
                    {
                        error(p, "geodesk:duplicate must have value 'yes'");
                    }
                }
                else if (*key == "geodesk:orphan")
                {
                    info.flags |= TagTableInfo::TAGGED_ORPHAN;
                    if (type != TagValueType::GLOBAL_STRING ||
                        (p-2).getUnsignedShort() != GlobalStrings::YES)
                    {
                        error(p, "geodesk:orphan must have value 'yes'");
                    }
                }
            }
            p -= 2 + (type & 2);
            if (!checkAccess(p, "tag table")) return info;
            checkTagValue(p, type);
            tagCount++;
            if (lastFlag) break;
        }
    }

    int syntheticTagCount = Bits::bitCount(
        info.flags & (TagTableInfo::TAGGED_ORPHAN | TagTableInfo::TAGGED_DUPLICATE));
    if (syntheticTagCount > 0 && tagCount > syntheticTagCount)
    {
        error(pTags, "geodesk:duplicate and geodesk:orphan "
            "must not appear with other tags");
    }
    return info;
}


void TileChecker::checkTagValue(DataPtr p, int type)
{
    if (type == TagValueType::LOCAL_STRING)
    {
        int rel = p.getInt();
        if (checkPointer(p, rel))
        {
            checkString(p + rel);
        }
    }
    else
    {
        // TODO: check numeric value (narrow number should not
        //  be encoded as wide number)
    }
}

const ShortVarString* TileChecker::checkString(DataPtr p)
{
    // TODO: Check for 0-padding to ensure minimum 4-byte element length
    if (!checkAccess(p + 1, "string")) return nullptr;
    const ShortVarString* s = reinterpret_cast<const ShortVarString*>(p.ptr());
    uint32_t totalSize = s->totalSize();
    if (!checkAccess(p + totalSize -1, "string")) return nullptr;

    // TODO: check content
    // TODO: check that string is not a global string
    // TODO: check string is not duplicated (warning only)

    return s;
}


bool TileChecker::checkFeatureBounds2D(FeaturePtr feature)
{
    if (!checkBounds(feature.ptr() - 16)) return false;
    Box bounds = feature.bounds();
    if (!bounds.intersects(tileBounds_))
    {
        error(feature.ptr(), "Feature lies outside of tile bounds");
        return false;
    }
    bool extendsWest = bounds.minX() < tileBounds_.minX();
    bool extendsSouth = bounds.minY() < tileBounds_.minY();
    bool extendsEast = bounds.maxX() > tileBounds_.maxX();
    bool extendsNorth = bounds.maxY() > tileBounds_.maxY();
    int edgesCrossed = extendsWest + extendsSouth + extendsEast + extendsNorth;
    if (edgesCrossed > 1)
    {
        error(feature.ptr(), "Feature extends past more than one tile edge");
        return false;
    }
    int multiTileFlags =
        (extendsWest ? FeatureFlags::MULTITILE_WEST : 0) |
        (extendsNorth ? FeatureFlags::MULTITILE_NORTH : 0);
    int flags = feature.flags();
    if ((flags & (FeatureFlags::MULTITILE_WEST |
        FeatureFlags::MULTITILE_NORTH)) != multiTileFlags)
    {
        error(feature.ptr(), "Invalid multi-tile flags");
        return false;
    }
    return true;
}


uint32_t TileChecker::checkFeature2D(FeaturePtr feature)
{
    checkId(feature);
    if (!checkFeatureBounds2D(feature)) return INVALID_INDEX;
    TagTableInfo tags = checkTagTablePtr(feature.ptr() + 8, feature.typedId());
    return tags.keys;
}

uint32_t TileChecker::checkWay(DataPtr p)
{
    WayPtr way(p);
    return checkFeature2D(way);
}

uint32_t TileChecker::checkRelation(DataPtr p)
{
    RelationPtr rel(p);
    return checkFeature2D(rel);
}

void TileChecker::checkExports(DataPtr ppExports)
{
    HashSet<TypedFeatureId> exported;
    int32_t rel = ppExports.getInt();
    if (rel == 0) return;
    if (!checkPointer(ppExports, rel)) return;
    DataPtr pTable = ppExports + rel;
    checkAccess(pTable-4, "export table");
    int count = (pTable-4).getInt();
    if (count == 0)
    {
        error(pTable-4, "Export table size must not be 0");
        return;
    }
    DataPtr p = pTable + (count - 1) * 4;
    checkAccess(p + 3, "export table");
    bool seenNonNull = false;
    do
    {
        rel = p.getInt();
        if (rel == 0)
        {
            if (!seenNonNull)
            {
                error("Null entries at end of export table");
                seenNonNull = true; // to suppress further errors
            }
        }
        else if (checkPointer(p, rel))
        {
            FeaturePtr feature(p + rel);
            TypedFeatureId typedId = feature.typedId();
            if (!features_.contains(typedId))
            {
                error(p, "Pointer to invalid exported feature");
            }
            else
            {
                auto [it, inserted] = exported.insert(typedId);
                if (!inserted)
                {
                    error(p, "Multiple TEXes assigned to %s/%ull",
                        feature.typeName(), feature.id());
                }
            }
            seenNonNull = true;
        }
        p -= 4;
    }
    while (p >= pTable);
}