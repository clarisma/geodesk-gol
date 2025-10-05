// Copyright (c) 2024 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/io/FileHandle.h>

namespace clarisma {

class CompressedData
{
public:
    static std::unique_ptr<CompressedData> create(const uint8_t* data, size_t size);

    std::byte* data() { return reinterpret_cast<std::byte*>(this) + sizeof(CompressedData); }
    const std::byte* data() const { return reinterpret_cast<const std::byte*>(this) + sizeof(CompressedData); }
    uint32_t payloadSize() const { return sizeCompressed_ + 8; }

    void writeTo(FileHandle handle) const
    {
        handle.writeAll(&sizeUncompressed_, sizeCompressed_ + 8);
    }

private:
    static void* operator new(std::size_t sz, std::size_t dataSize)
    {
        return ::operator new(sz + dataSize);
    }

    static void operator delete(void* p) noexcept
    {
        ::operator delete(p);
    }

    uint32_t sizeCompressed_;
    uint32_t sizeUncompressed_;
    uint32_t checksum_;
};
    

} // namespace clarisma

