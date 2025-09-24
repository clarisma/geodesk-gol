// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <geodesk/feature/ForeignFeatureRef.h>

class TFeature;

using namespace geodesk;

class FeatureRef
{
public:
    FeatureRef() : data_(0) {}
    FeatureRef(TFeature* feature) : data_(reinterpret_cast<uintptr_t>(feature)) {}
    FeatureRef(ForeignFeatureRef ref) :
        data_((static_cast<uint64_t>(ref.tex) << 32) |
            (static_cast<uint64_t>(ref.tip) << 1) | 1) {}

    bool isForeign() const { return data_ & 1; }
    bool isNull() const { return data_ == 0; }

    Tip tip() const
    {
        assert(isForeign());
        return static_cast<Tip>(static_cast<uint32_t>(data_) >> 1);
    }

    Tex tex() const
    {
        assert(isForeign());
        return static_cast<Tex>(static_cast<uint32_t>(data_ >> 32));
    }

    TFeature* local() const
    {
        assert(!isForeign());
        return reinterpret_cast<TFeature*>(data_);
    }


private:
    uintptr_t data_;
};
