// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/util/BufferWriter.h>

class PropertyTableBuilder : public clarisma::BufferWriter
{
public:
    PropertyTableBuilder() :
        buf_(4096),
        count_(0)
    {
        setBuffer(&buf_);
        writeByte(0);
        writeByte(0);
    }

    void add(std::string_view name, std::string_view value)
    {
        writeVarint(name.size());
        writeString(name);
        writeVarint(value.size());
        writeString(value);
        count_++;
    }

    clarisma::ByteBlock take()
    {
        flush();
        uint16_t* p = reinterpret_cast<uint16_t*>(buf_.start());
        *p = static_cast<uint16_t>(count_);
        return buf_.takeBytes();
    }

private:
    int count_;
    clarisma::DynamicBuffer buf_;
};
