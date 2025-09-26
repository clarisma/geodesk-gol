// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/data/SmallArray.h>
#include <geodesk/feature/FeaturePtr.h>
#include <geodesk/format/KeySchema.h>
#include <geodesk/format/StringHolder.h>

using namespace geodesk;

class FeatureRow : public clarisma::SmallArray<StringHolder,32>
{
public:
    FeatureRow(const KeySchema& keys, FeatureStore* store,
        FeaturePtr feature, int precision);
};

