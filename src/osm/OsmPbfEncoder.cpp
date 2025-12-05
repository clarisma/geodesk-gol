// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include "OsmPbfEncoder.h"
#include "ProtoPbfEncoder.h"
#include <clarisma/util/Pointers.h>



std::unique_ptr<uint8_t[]> OsmPbfEncoder::createStringTable(
    int stringCount, uint8_t* strings)
{
    struct SortableCounter
    {
        uint32_t count;
        uint32_t ofs;
    };

    uint8_t* p = strings;
    uint32_t totalSize = 0;

    std::vector<SortableCounter> sorted;
    sorted.reserve(stringCount);

    for (int i=0; i<stringCount; i++)
    {
        ProtoPbfEncoder::StringCounter* counter =
            reinterpret_cast<ProtoPbfEncoder::StringCounter*>(p);
        sorted.emplace_back(counter->count, Pointers::offset32(p, strings));
        uint32_t totalStringSize = counter->string.totalSize();
        totalSize += totalStringSize + 1;
        p += ProtoPbfEncoder::StringCounter::calculateTotalSize(totalStringSize);
    }

}
