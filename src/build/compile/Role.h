// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <cassert>
#include <cstdint>

class TString;

class Role
{
public:
    Role(int code, TString* str)
    {
        if (code < 0)
        {
            data_ = reinterpret_cast<uintptr_t>(str);
        }
        else
        {
            data_ = (code << 1) | 1;
        }
    }

    bool isGlobal() const
    {
        return data_ & 1;
    }

    int code() const
    {
        assert(isGlobal());
        return static_cast<int>(data_ >> 1);
    }

    TString* localString() const
    {
        assert(!isGlobal());
        return reinterpret_cast<TString*>(data_);
    }

    bool operator==(const Role& other) const
    {
        return data_ == other.data_;
    }

    bool operator!=(const Role& other) const
    {
        return data_ != other.data_;
    }

private:
    uintptr_t data_;
};