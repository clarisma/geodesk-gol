// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include "ProtoPbfEncoder.h"
#include <clarisma/util/varint.h>
#include <geodesk/format/FilteredTagWalker.h>

ProtoPbfEncoder::ProtoPbfEncoder()
{
}

uint32_t ProtoPbfEncoder::addGlobalString(int code, const ShortVarString* s)
{
    uint32_t ofs = globalStringIndex_[code];
    if (ofs)
    {
        reinterpret_cast<StringCounter*>(data_.get() + ofs)->count++;
        return ofs;
    }
    StringCounter* counter = createCounter(s);
    if (!counter) [[unlikely]] return 0;
    ofs = counterOfs(counter);
    globalStringIndex_[code] = ofs;
    recentStrings_.push_back(counter);
    return ofs;
}


uint32_t ProtoPbfEncoder::addLocalString(const ShortVarString* s)
{
    auto it = localStringIndex_.find(s);
    if (it != localStringIndex_.end())
    {
        it->second->count++;
        return counterOfs(it->second);
    }
    StringCounter* counter = createCounter(s);
    if (!counter) [[unlikely]] return 0;
    localStringIndex_.emplace(s, counter);
    recentStrings_.push_back(counter);
    return counterOfs(counter);
}

ProtoPbfEncoder::StringCounter* ProtoPbfEncoder::createCounter(const ShortVarString* s)
{
    uint32_t totalStringSize = s->totalSize();
    uint32_t totalSize = StringCounter::calculateTotalSize(totalStringSize);
    if (pStringsStart_ - totalSize < pFeaturesEnd_) return nullptr;
    pStringsStart_ -= totalSize;
    auto counter = reinterpret_cast<StringCounter*>(pStringsStart_);
    counter->count = 1;
    memcpy(&counter->string, s, totalStringSize);
    return counter;
}


bool ProtoPbfEncoder::addTags(uint8_t*& p, const uint8_t* pEnd, TagTablePtr tags)
{
    char buf[32];
    FilteredTagWalker tw(tags, strings_, keySchema_);
    for (;;)
    {
        if (tw.next() == 0) break;
        if (p >= pEnd) [[unlikely]] return false;
        uint32_t keyOfs;
        if (tw.keyCode() >= 0)  [[likely]]
        {
            keyOfs = addGlobalString(tw.keyCode(), tw.key());
        }
        else
        {
            keyOfs = addLocalString(tw.key());
        }
        if (!keyOfs) return false;

        uint32_t valueOfs;
        TagValueType valueType = tw.valueType();
        if (valueType == TagValueType::GLOBAL_STRING)   [[likely]]
        {
            int stringCode = static_cast<int>(tw.narrowValueFast());
            valueOfs = addGlobalString(stringCode, strings_.getGlobalString(stringCode));
        }
        else
        {
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
            valueOfs = addLocalString(s);
        }
        if (!valueOfs) return false;

        writeVarint(p, (keyOfs << 1) | 1);
        writeVarint(p, valueOfs);
    }
    *p++ = 0;
    return true;
}


bool ProtoPbfEncoder::addWay(geodesk::WayPtr way)
{
    recentStrings_.clear();
    uint8_t* p = pFeaturesEnd_;
    const uint8_t* pEnd = pStringsStart_ - 64;

    if (p >= pEnd) [[unlikely]] return false;

    TagTablePtr tags = way.tags();
    bool hasTags = !tags.isEmpty();
    *p++ = hasTags;
    writeVarint(p, way.id());
    if (!addTags(p, pEnd, tags)) return false;



}