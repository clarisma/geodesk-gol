// Copyright (c) 2024 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include <clarisma/util/DateTime.h>
#ifdef CLARISMA_WITH_ZLIB

#pragma once

#include <clarisma/alloc/Block.h>
#include <clarisma/zip/ZipException.h>

namespace clarisma {

namespace Zip
{
ByteBlock deflate(const uint8_t* data, size_t size);
inline ByteBlock deflate(const ByteBlock& block)
{
    return deflate(block.data(), block.size());
}

ByteBlock deflateRaw(const uint8_t* data, size_t size);
inline ByteBlock deflateRaw(const ByteBlock& block)
{
    return deflateRaw(block.data(), block.size());
}

ByteBlock inflate(const uint8_t* data, size_t size, size_t sizeUncompressed);
inline ByteBlock inflate(const ByteBlock& block, size_t sizeUncompressed)
{
    return inflate(block.data(), block.size(), sizeUncompressed);
}

ByteBlock inflateRaw(const uint8_t* data, size_t size, size_t sizeUncompressed);
inline ByteBlock inflateRaw(const ByteBlock& block, size_t sizeUncompressed)
{
    return inflateRaw(block.data(), block.size(), sizeUncompressed);
}

uint32_t calculateChecksum(const ByteBlock& block);
void verifyChecksum(const ByteBlock& block, uint32_t checksum);

struct GzipMetadata
{
    const char* fileName = nullptr;
    const char* comment = nullptr;
    const uint8_t* extraData = nullptr;
    uint32_t extraDataSize = 0;
    int32_t timestamp = 0;     // seconds since Unix epoch
    const uint8_t* compressedData = nullptr;
    uint32_t uncompressedSize = 0;
    uint32_t checksum = 0;
};

void getGzipMetadata(const uint8_t* data, size_t size, GzipMetadata* meta);
ByteBlock inflateGzip(const uint8_t* data, size_t size);

inline ByteBlock inflateGzip(const std::byte* data, size_t size)
{
    return inflateGzip(reinterpret_cast<const uint8_t*>(data), size);
}

template<typename Container>
ByteBlock inflateGzip(const Container& container)
{
    return inflateGzip(container.data(), container.size());
}

inline ByteBlock inflateGzip(const ByteBlock& block)
{
    return inflateGzip(block.data(), block.size());
}

}
    

} // namespace clarisma

#endif