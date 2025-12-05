// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <cstdint>
#include <memory>

struct StringCounter;

class OsmPbfEncoder
{
private:
    static std::unique_ptr<uint8_t[]> createStringTable(
        int stringCount, uint8_t* strings);
};

