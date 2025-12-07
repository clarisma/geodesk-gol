// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include "OsmPbfEncoder.h"
#include <clarisma/util/Pointers.h>
#include <geodesk/format/FilteredTagWalker.h>
#include "OsmPbf.h"
#include "geodesk/feature/FastMemberIterator.h"
#include "geodesk/feature/MemberIterator.h"


int OsmPbfEncoder::addString(const ShortVarString* s)
{
    int n = stringCount_;
    uint32_t totalStringSize = s->totalSize();
    if (pStrings_ + totalStringSize >= pStringsEnd_) [[unlikely]] return -1;
        // we use >= instead of > because we need to account for the
        // 1-byte string-entry tag (i.e. a 12-byte string needs 14 bytes)
    *pStrings_++ = OsmPbf::STRINGTABLE_ENTRY;
    memcpy(pStrings_, s, totalStringSize);
    pStrings_ += totalStringSize;
    stringCount_++;
    return n;
}

int OsmPbfEncoder::getGlobalString(int code, const ShortVarString* s)
{
    int n = globalStringIndex_[code];
    if (n >= 0) return n;
    n = addString(s);
    globalStringIndex_[code] = n;
    return n;
}


int OsmPbfEncoder::getLocalString(const ShortVarString* s)
{
    auto it = localStringIndex_.find(s);
    if (it != localStringIndex_.end())
    {
        return it->second;
    }
    int n = addString(s);
    localStringIndex_.emplace(s, n);
        // This is harmless, because if n < 0, the buffers
        // are full and we won't be encoding any more features
    return n;
}


OsmPbfEncoder::Tag OsmPbfEncoder::getTag(FilteredTagWalker& tw)
{
    int k;
    if (tw.keyCode() >= 0)  [[likely]]
    {
        k = getGlobalString(tw.keyCode(), tw.key());
    }
    else
    {
        k = getLocalString(tw.key());
    }
    if (k < 0) return {-1,-1};

    int v;
    TagValueType valueType = tw.valueType();
    if (valueType == TagValueType::GLOBAL_STRING)   [[likely]]
    {
        int stringCode = static_cast<int>(tw.narrowValueFast());
        v = getGlobalString(stringCode,
            strings_.getGlobalString(stringCode));
    }
    else
    {
        char buf[32];
        const ShortVarString* s;
        if (valueType == TagValueType::LOCAL_STRING)
        {
            s = tw.localStringValueFast();
        }
        else
        {
            const char* end = tw.numberValueFast().format(&buf[1]);
            buf[0] = static_cast<char>(end - &buf[1]);
            s = reinterpret_cast<const ShortVarString*>(&buf);
        }
        v = getLocalString(s);
    }
    return { v < 0 ? v : k, v };
}


bool OsmPbfEncoder::addTags(TagTablePtr tags)
{
    keys_.clear();
    values_.clear();
    FilteredTagWalker tw(tags, strings_, keySchema_);
    for (;;)
    {
        if (tw.next() == 0) break;
        Tag tag = getTag(tw);
        if (tag.key < 0) [[unlikely]] return false;
        keys_.writeVarint(tag.key);
        values_.writeVarint(tag.value);
    }
    return true;
}


bool OsmPbfEncoder::addNode(NodePtr node)
{
    uint8_t* pPrevStrings = pStrings_;
    uint8_t* pPrevTags = pTags_;
    bool hasTags = false;
    FilteredTagWalker tw(node.tags(), strings_, keySchema_);
    while (tw.next() != 0)
    {
        Tag tag = getTag(tw);
        if (tag.key < 0 || pTags_ + 10 > pTagsEnd_) [[unlikely]]
        {
            // There must be room for at least 2 32-bit varints
            pStrings_ = pPrevStrings;
            pTags_ = pPrevTags;
            return false;
        }
        writeVarint(pTags_, tag.key);
        writeVarint(pTags_, tag.value);
        hasTags = true;
    }
    if (!addNode(static_cast<int64_t>(node.id()), node.xy())) [[unlikely]]
    {
        pStrings_ = pPrevStrings;
        pTags_ = pPrevTags;
        return false;
    }
    anyNodesHaveTags_ |= hasTags;
    return true;
}

bool OsmPbfEncoder::addNode(int64_t id, Coordinate xy)
{
    if (p_ + 10 > pEnd_) [[unlikely]] return false;
    if (pLats_ + 10 > pLatsEnd_) [[unlikely]] return false;
    if (pLons_ + 10 > pLonsEnd_) [[unlikely]] return false;
    if (pTags_ == pTagsEnd_) [[unlikely]] return false;
    writeSignedVarint(p_, id - prevId_);
    prevId_ = id;
    int32_t lon = Mercator::lon100ndFromX(xy.x);
    int32_t lat = Mercator::lat100ndFromY(xy.y);
    writeSignedVarint(pLons_, lon - prevLon_);
    prevLon_ = lon;
    writeSignedVarint(pLats_, lat - prevLat_);
    prevLat_ = lat;
    *pTagsEnd_++ = 0;
    return true;
}

void OsmPbfEncoder::writeBuffer(int tag, const Buffer& buf)
{
    *p_++ = tag;
    size_t size = buf.length();
    writeVarint(p_, size);
    memcpy(p_,buf.data(), size);
    p_ += size;
    assert(p_ <= pEnd_);
}

bool OsmPbfEncoder::addWay(WayPtr way)
{

}

bool OsmPbfEncoder::addRelation(RelationPtr rel)
{
    uint8_t* pPrevStrings = pStrings_;
    nodesOrRoles_.clear();
    latsOrMembers_.clear();
    lonsOrTypes_.clear();
    MemberIterator iter(store_, rel.bodyptr());
    int64_t prevMemberId = 0;
    for (;;)
    {
        FeaturePtr member = iter.next();
        if (member.isNull()) break;

        int role;
        int roleCode = iter.currentRoleCode();
        const ShortVarString* roleString = static_cast<const ShortVarString*>(
            iter.currentRoleStr());
        if (roleCode >= 0) [[likely]]
        {
            role = getGlobalString(roleCode, roleString);
        }
        else
        {
            role = getLocalString(roleString);
        }
        if (role < 0)   [[unlikely]]
        {
            pStrings_ = pPrevStrings;
            return false;
        }
        nodesOrRoles_.writeVarint(role);

        int64_t memberId = static_cast<int64_t>(member.id());
        latsOrMembers_.writeSignedVarint(memberId - prevMemberId);
        prevMemberId = memberId;
        lonsOrTypes_.writeByte(member.typeCode());
    }

    if (!addTags(rel.tags())) [[unlikely]]
    {
        pStrings_ = pPrevStrings;
        return false;
    }

    uint64_t id = rel.id();

    size_t keysSize = keys_.length();
    size_t valuesSize = values_.length();
    size_t rolesSize = nodesOrRoles_.length();
    size_t membersSize = latsOrMembers_.length();
    size_t typesSize = lonsOrTypes_.length();

    size_t totalSize = varintSize(id) +
        keysSize + varintSize(keysSize) +
        valuesSize + varintSize(valuesSize) +
        rolesSize + varintSize(rolesSize) +
        membersSize + varintSize(membersSize) +
        typesSize + varintSize(typesSize) + 6;

    if (p_ + totalSize + 10 >= pEnd_)   [[unlikely]]
    {
        pStrings_ = pPrevStrings;
        return false;
    }

    *p_++ = OsmPbf::GROUP_RELATION;
    writeVarint(p_, totalSize);
    const uint8_t* pBody = p_;
    *p_++ = OsmPbf::ELEMENT_ID;
    writeVarint(p_, id);
    writeBuffer(OsmPbf::ELEMENT_KEYS, keys_);
    writeBuffer(OsmPbf::ELEMENT_VALUES, values_);
    writeBuffer(OsmPbf::RELATION_MEMBER_ROLES, nodesOrRoles_);
    writeBuffer(OsmPbf::RELATION_MEMBER_IDS, latsOrMembers_);
    writeBuffer(OsmPbf::RELATION_MEMBER_TYPES, lonsOrTypes_);
    assert(p_ - Body_ == totalSize);

    return true;
}