// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/util/Buffer.h>
#include <clarisma/util/varint.h>

class VarintEncoder final : public clarisma::DynamicBuffer
{
public:
    explicit VarintEncoder(size_t initialCapacity = 1024) :
        DynamicBuffer(initialCapacity) {}

    void writeVarint(uint64_t value)
    {
        ensureCapacityUnsafe(11);
            // 10 max bytes + 1 to ensure not full after write
        clarisma::writeVarint(reinterpret_cast<uint8_t*&>(p_), value);
    }

    void writeSignedVarint(int64_t value)
    {
        ensureCapacityUnsafe(11);
        // 10 max bytes + 1 to ensure not full after write
        clarisma::writeSignedVarint(reinterpret_cast<uint8_t*&>(p_), value);
    }
};

