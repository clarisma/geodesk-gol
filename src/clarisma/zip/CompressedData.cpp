// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <clarisma/zip/CompressedData.h>
#include <cassert>
#include <zlib.h>
#include <clarisma/util/Crc32C.h>

namespace clarisma {

std::unique_ptr<CompressedData> CompressedData::create(const uint8_t* data, size_t size)
{
    assert(size <= 0xffff'ffff);
    uLong maxCompressedSize = compressBound(size);
    assert(maxCompressedSize <= 0xffff'ffff);

    CompressedData* compressed = new (maxCompressedSize) CompressedData();

    // Prepare z_stream for raw DEFLATE (no header, no footer).
    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.next_in =
        const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
    zs.avail_in = static_cast<uInt>(size);

    // Init with raw deflate: windowBits = -MAX_WBITS.
    int res = deflateInit2(&zs,
        Z_DEFAULT_COMPRESSION,
        Z_DEFLATED,
        -MAX_WBITS,
        8,
        Z_DEFAULT_STRATEGY);
    if (res != Z_OK)
    {
        delete compressed;
        throw ZipException(res);
    }

    compressed->checksum_ = Crc32C::compute(data, size);

    // Compress into tail buffer.
    zs.next_out = reinterpret_cast<Bytef*>(compressed->data());
    zs.avail_out = maxCompressedSize;

    res = deflate(&zs, Z_FINISH);
    compressed->sizeUncompressed_ = static_cast<uint32_t>(size);
    compressed->sizeCompressed_ = static_cast<uint32_t>(
        maxCompressedSize - zs.avail_out);
    deflateEnd(&zs);
    if (res != Z_STREAM_END)
    {
        delete compressed;
        throw ZipException(res);
    }
    return std::unique_ptr<CompressedData>(compressed);
}


} // namespace clarisma

