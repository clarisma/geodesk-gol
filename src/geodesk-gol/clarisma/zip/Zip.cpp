// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#ifdef CLARISMA_WITH_ZLIB

#include <clarisma/zip/Zip.h>
#include <zlib.h>
#include <clarisma/alloc/ReusableBlock.h>
#include <clarisma/util/Crc32C.h>
#include <clarisma/util/log.h>

namespace clarisma {

namespace Zip
{
ByteBlock deflate(const uint8_t* data, size_t size)
{
    uLongf compressedSize = compressBound(size);
    std::unique_ptr<uint8_t[]> compressed =
        std::make_unique<uint8_t[]>(compressedSize);
    int res = compress(compressed.get(), &compressedSize, data, size);
    if (res != Z_OK)
    {
        throw ZipException(res);
    }
    return ByteBlock(std::move(compressed), compressedSize);
}

ByteBlock deflateRaw(const uint8_t* data, size_t size)
{
    z_stream strm{};
    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
    strm.avail_in = static_cast<uInt>(size);

    uLongf bound = compressBound(size);
    std::unique_ptr<uint8_t[]> out =
        std::make_unique<uint8_t[]>(bound);

    strm.next_out = out.get();
    strm.avail_out = static_cast<uInt>(bound);

    // windowBits < 0 → raw deflate (no header, no checksum)
    int ret = deflateInit2(
        &strm,
        Z_DEFAULT_COMPRESSION,
        Z_DEFLATED,
        -MAX_WBITS,
        8,
        Z_DEFAULT_STRATEGY);

    if (ret != Z_OK)
    {
        throw ZipException(ret);
    }

    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END)
    {
        deflateEnd(&strm);
        throw ZipException(ret);
    }

    size_t compressedSize = strm.total_out;
    deflateEnd(&strm);

    return ByteBlock(std::move(out), compressedSize);
}

ByteBlock inflate(const uint8_t* data, size_t size, size_t sizeUncompressed)
{
    // Allocate memory for the uncompressed data
    std::unique_ptr<uint8_t[]> uncompressedData(new uint8_t[sizeUncompressed]);

    /*
    // Initialize the zlib stream
    z_stream stream;
    stream.avail_in = static_cast<uInt>(size);
    stream.next_in = const_cast<uint8_t*>(data);
    stream.avail_out = static_cast<uInt>(sizeUncompressed);
    stream.next_out = uncompressedData.get();

    // Initialize the inflation process
    int ret = inflateInit(&stream);
    if (ret != Z_OK)
    {
        throw ZipException(ret);
    }

    // Inflate the data
    ret = inflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END)
    {
        inflateEnd(&stream);
        throw ZipException(ret);
    }

    // Clean up
    inflateEnd(&stream);
    

    // Verify the uncompressed size
    if (stream.total_out != sizeUncompressed)
    {
        throw ZipException("Uncompressed size does not match expected size");
    }
    */

    uLongf uncompressedSizeZlib = sizeUncompressed;
    int res = uncompress(uncompressedData.get(), &uncompressedSizeZlib, data, size);
    if (res != Z_OK)
    {
        throw ZipException(res);
    }
    return ByteBlock(std::move(uncompressedData), sizeUncompressed);
}


uint32_t calculateChecksum(const ByteBlock& block)
{
    return crc32_z(0, block.data(), static_cast<z_size_t>(block.size()));
}

void verifyChecksum(const ByteBlock& block, uint32_t checksum)
{
    if (calculateChecksum(block) != checksum)
    {
        throw ZipException("Checksum mismatch");
    }
}


ByteBlock inflateRaw(const uint8_t* data, size_t size, size_t sizeUncompressed)
{
    // Allocate memory for the uncompressed data
    std::unique_ptr<uint8_t[]> uncompressedData(new uint8_t[sizeUncompressed]);

    z_stream strm{};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = static_cast<uInt>(size);
    strm.next_in = const_cast<Bytef*>(data);

    // Initialize inflate for raw DEFLATE (negative windowBits)
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
    {
        throw ZipException(Z_ERRNO);
    }

    strm.avail_out = static_cast<uInt>(sizeUncompressed);
    strm.next_out = uncompressedData.get();

    // Inflate raw DEFLATE data
    int ret = inflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END)
    {
        inflateEnd(&strm);
        throw ZipException(ret);
    }

    // Verify that the uncompressed size matches the expected size
    if (strm.total_out != sizeUncompressed)
    {
        inflateEnd(&strm);
        throw ZipException(Z_BUF_ERROR);
    }

    // Clean up the inflate stream
    inflateEnd(&strm);

    // Return the uncompressed data wrapped in ByteBlock
    return ByteBlock(std::move(uncompressedData), sizeUncompressed);
}

ByteBlock compressSealedChunk(const uint8_t* data, size_t size)
{
    assert(size <= 0xffff'ffff);
    uLong maxCompressedSize = compressBound(size);
    assert(maxCompressedSize <= 0xffff'ffff);

    std::unique_ptr<uint8_t[]> out =
        std::make_unique<uint8_t[]>(maxCompressedSize + 8);

    uint8_t* p = out.get();

    // Prepare z_stream for raw DEFLATE (no header, no footer).
    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
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
        throw ZipException(res);
    }

    *reinterpret_cast<uint32_t*>(p) = size;
    *reinterpret_cast<uint32_t*>(p + 4) = Crc32C::compute(data, size);

    // Compress into tail buffer.
    zs.next_out = reinterpret_cast<Bytef*>(p + 8);
    zs.avail_out = maxCompressedSize;

    res = deflate(&zs, Z_FINISH);
    size_t compressedSize = maxCompressedSize - zs.avail_out;
    deflateEnd(&zs);
    if (res != Z_STREAM_END)
    {
        throw ZipException(res);
    }
    return ByteBlock(std::move(out), compressedSize + 8);
}

ByteBlock uncompressSealedChunk(const uint8_t* data, size_t size)
{
    uint32_t sizeUncompressed = *reinterpret_cast<const uint32_t*>(data);
    uint32_t checksum = *reinterpret_cast<const uint32_t*>(data + 4);
    LOGS << "Uncompressing chunk of " << size << " bytes to to " <<
        sizeUncompressed << " bytes (uncompressed)";
    ByteBlock uncompressed = inflateRaw(data + 8, size - 8, sizeUncompressed);
    if (Crc32C::compute(uncompressed.data(), uncompressed.size()) != checksum)
    {
        throw ZipException("Checksum mismatch");
    }
    return uncompressed;
}

}  // end namespace Zip


} // namespace clarisma

#endif