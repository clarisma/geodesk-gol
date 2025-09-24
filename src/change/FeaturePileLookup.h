// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <clarisma/store/IndexFile.h>
#include <geodesk/feature/FeatureType.h>

using namespace geodesk;

class FeaturePileLookup
{
public:
    bool open(std::string_view golPath, int tileCount);

    int get(FeatureType type, uint64_t id)
    {
        int typeCode = static_cast<int>(type);
        return static_cast<int>(indexes_[typeCode].get(id));
    }

private:
    clarisma::IndexFile indexes_[3];
};


