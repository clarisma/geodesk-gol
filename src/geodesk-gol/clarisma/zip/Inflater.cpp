// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#ifdef CLARISMA_WITH_ZLIB

#include <clarisma/zip/Inflater.h>
#include <cassert>
#include <zlib.h>
#include <clarisma/util/Crc32C.h>
#include <clarisma/zip/ZipException.h>

namespace clarisma {

Inflater::Inflater(uint32_t bufferSize) :
    bufferSize_(bufferSize)
{
    buffer_.reset(new byte[bufferSize]);
}

void Inflater::inflateRaw(FileHandle file, uint64_t ofs, uint32_t srcSize,
    byte* dest, uint32_t destSize, uint32_t expectedCrc)
{
    z_stream zs = {};
    int res = ::inflateInit2(&zs, -15);  // raw DEFLATE
    if (res != Z_OK) throw ZipException(res);

    uint32_t inputRemaining = srcSize;
    zs.next_out = reinterpret_cast<Bytef*>(dest);
    zs.avail_out = static_cast<uInt>(destSize);

    Crc32C crc;

    for (;;)
    {
        size_t bytesRead;
        bool readOk = file.tryReadAt(ofs, buffer_.get(),
            std::min(bufferSize_, inputRemaining),
            bytesRead);
        if (bytesRead == 0) [[unlikely]]
        {
            ::inflateEnd(&zs);
            if (!readOk) throw IOException();
            throw IOException(inputRemaining ?
                "Unexpected EOF" :
                "Compressed data size mismatch");
        }

        ofs += bytesRead;
        inputRemaining -= bytesRead;

        auto prev = zs.next_out;
        zs.next_in = reinterpret_cast<Bytef*>(buffer_.get());
        zs.avail_in = static_cast<uInt>(bytesRead);
        res = ::inflate(&zs, inputRemaining ? Z_NO_FLUSH : Z_FINISH);
        crc.update(prev, zs.next_out - prev);
        if (res != Z_OK) break;
    }
    auto bufferRemaining = zs.avail_out;
    ::inflateEnd(&zs);
        // inflateEnd only reports usage errors, not data errors

    if (res != Z_STREAM_END)
    {
        throw ZipException(res);
    }
    if (inputRemaining || bufferRemaining)
    {
        throw ZipException("Compressed data size mismatch");
    }
    if (crc.get() != expectedCrc)
    {
        throw ZipException("Checksum mismatch");
    }
}

} // namespace clarisma

#endif