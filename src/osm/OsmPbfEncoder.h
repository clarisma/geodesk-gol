// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <memory>
#include <clarisma/data/ShortVarStringMap.h>
#include <geodesk/feature/NodePtr.h>
#include <geodesk/feature/WayPtr.h>
#include <geodesk/feature/RelationPtr.h>
#include <geodesk/format/KeySchema.h>

#include "OsmPbf.h"
#include "VarintEncoder.h"

namespace geodesk {
class FilteredTagWalker;
}

using namespace clarisma;
using namespace geodesk;

class OsmPbfEncoder
{
public:
    struct Manifest
    {
        const uint8_t* pStrings;
        const uint8_t* pFeatures;
        const uint8_t* pNodeLons;
        const uint8_t* pNodeLats;
        const uint8_t* pNodeTags;
        uint32_t stringsSize;
        uint32_t featuresSize;
        uint32_t nodeLonsSize;
        uint32_t nodeLatsSize;
        uint32_t nodeTagsSize;
    };

    struct GroupCode
    {
        enum
        {
            NODES = OsmPbf::GROUP_DENSENODES,
            WAYS = OsmPbf::GROUP_WAY,
            RELATIONS = OsmPbf::GROUP_RELATION
        };
    };

    std::unique_ptr<uint8_t[]> start(int groupCode);
    bool addNode(NodePtr node);
    bool addNode(int64_t id, Coordinate xy);
    bool addWay(WayPtr way);
    bool addRelation(RelationPtr rel);
    std::unique_ptr<uint8_t[]> takeBlock()
    {
        assert(block_);
        finishBlock();
        return std::move(block_);
    }

private:
    struct Tag
    {
        int key;
        int value;
    };

    int getGlobalString(int code, const ShortVarString* s);
    int getLocalString(const ShortVarString* s);
    int addString(const ShortVarString* s);
    Tag getTag(FilteredTagWalker& tw);
    bool addTags(TagTablePtr tags);
    void writeBuffer(int tag, const Buffer& buf);
    void finishBlock();

    static constexpr int BLOCK_SIZE = 16 * 1024 * 1024;

    std::unique_ptr<uint8_t[]> block_;
    uint8_t* p_ = nullptr;
    uint8_t* pEnd_ = nullptr;
    uint8_t* pStrings_ = nullptr;
    uint8_t* pStringsEnd_ = nullptr;
    uint8_t* pLats_ = nullptr;
    uint8_t* pLatsEnd_ = nullptr;
    uint8_t* pLons_ = nullptr;
    uint8_t* pLonsEnd_ = nullptr;
    uint8_t* pTags_ = nullptr;
    uint8_t* pTagsEnd_ = nullptr;
    VarintEncoder keys_;
    VarintEncoder values_;
    VarintEncoder nodesOrRoles_;
    VarintEncoder latsOrMembers_;
    VarintEncoder lonsOrTypes_;
    std::unique_ptr<int[]> globalStringIndex_;
    ShortVarStringMap<int> localStringIndex_;
    FeatureStore* store_ = nullptr;
    StringTable& strings_;
    KeySchema* keySchema_;
    int groupCode_ = 0;
    int stringCount_ = 0;
    int64_t prevId_ = 0;
    int32_t prevLon_ = 0;
    int32_t prevLat_ = 0;
    bool anyNodesHaveTags_ = false;
};

