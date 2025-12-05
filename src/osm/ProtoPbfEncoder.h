// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <cstdint>
#include <clarisma/data/ShortVarStringMap.h>
#include <clarisma/util/Bytes.h>
#include <geodesk/feature/WayPtr.h>

#include "geodesk/format/KeySchema.h"

using namespace clarisma;
using namespace geodesk;

class ProtoPbfEncoder
{
public:
    bool addWay(WayPtr way);

    struct StringCounter
    {
        uint8_t count;
        ShortVarString string;

        static uint32_t calculateTotalSize(uint32_t totalStringSize)
        {
            return Bytes::aligned(totalStringSize
                + sizeof(count), 4);
        }
    };

private:
    uint32_t addGlobalString(int code, const ShortVarString* s);
    uint32_t addLocalString(const ShortVarString* s);
    StringCounter* createCounter(const ShortVarString* s);
    uint32_t counterOfs(const StringCounter* c) const
    {
        return reinterpret_cast<const uint8_t*>(c) - data_.get();
    }

    bool addTags(uint8_t*& p, const uint8_t* pEnd, TagTablePtr tags);

    std::unique_ptr<uint8_t[]> data_;
    uint32_t* pFeaturesEnd_;
    uint32_t* pStringsStart_;
    std::unique_ptr<uint32_t[]> globalStringIndex_;
    ShortVarStringMap<StringCounter*> localStringIndex_;
    StringTable& strings_;
    KeySchema* keySchema_;
    std::vector<StringCounter*> recentStrings_;
};
