// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#include <cassert>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <zlib.h>
#include "ZipException.h"

/// @brief Incremental zlib deflater with known total input size.
/// @details
/// - Construct with total uncompressed size.
/// - Call addChunk() any number of times (summing to <= total size).
/// - Call finish() once; after that, compressedData() holds the result.
/// - Internally, a single fixed buffer of size deflateBound(totalSize)
///   is used; there are no temporary output chunks and no reallocation.


// Deflater.h
//
// Simple wrapper around zlib's deflate() to compress data in chunks,
// given the total uncompressed size up front.

namespace clarisma {

/// @brief Incremental zlib deflater with known total input size.
/// @details
/// - Construct with total uncompressed size.
/// - Call addChunk() any number of times (summing to <= total size).
/// - Call finish() once; after that, compressedData() holds the result.
/// - Internally, a single fixed buffer of size deflateBound(totalSize)
///   is used; there are no temporary output chunks and no reallocation.
class Deflater
{
public:
    /// @brief Create a deflater.
    /// @param uncompressedSize Total input size in bytes (expected).
    /// @param level zlib compression level (Z_DEFAULT_COMPRESSION, etc.).
    explicit Deflater(std::size_t uncompressedSize,
        int level = Z_DEFAULT_COMPRESSION) :
         compressionLevel_(level)
    {
        std::memset(&stream_, 0, sizeof(stream_));

        int windowBits = 15;   // 32K window, zlib header
        int memLevel = 8;      // default

        int ret = deflateInit2(
            &stream_,
            level,
            Z_DEFLATED,
            windowBits,
            memLevel,
            Z_DEFAULT_STRATEGY);
        if (ret != Z_OK)
        {
            throw ZipException(ret);
        }
        initialized_ = true;

        // Ask zlib for an upper bound for this total size, using the
        // exact stream settings (windowBits, etc.).
        uLong sourceLen = static_cast<uLong>(uncompressedSize);
        uLong bound = deflateBound(&stream_, sourceLen);

        buf_.reset(new uint8_t[bound]);
        stream_.next_out = reinterpret_cast<Bytef*>(buf_.get());
    }

    /// @brief Destructor, releases zlib resources if needed.
    ~Deflater()
    {
        if(initialized_) deflateEnd(&stream_);
    }

    Deflater(const Deflater&) = delete;
    Deflater& operator=(const Deflater&) = delete;
    Deflater(Deflater&&) = delete;
    Deflater& operator=(Deflater&&) = delete;

    /// @brief Add an input chunk to be compressed.
    /// @param data Pointer to input bytes.
    /// @param size Number of bytes in the chunk.
    /// @throws std::logic_error if called after finish().
    /// @throws std::runtime_error on overflow or zlib error.
    void deflate(const void* data, std::size_t size)
    {
        assert (initialized_);
        stream_.next_in = const_cast<Bytef*>(
            reinterpret_cast<const Bytef*>(data));
        stream_.avail_in = static_cast<uInt>(size);
        int ret = ::deflate(&stream_, Z_NO_FLUSH);
        if (ret != Z_OK) throw ZipException(ret);
    }

    /// @brief Finish the stream and flush remaining compressed data.
    /// @details
    /// After this call, no further input is allowed.
    /// @throws std::runtime_error on zlib error or overflow.
    void finish()
    {
        assert (initialized_);
        int ret = inflate(&stream_, Z_FINISH);
        if (ret != Z_STREAM_END) throw ZipException(ret);
        deflateEnd(&stream_);
        initialized_ = false;
    }

private:
    z_stream stream_;
    std::unique_ptr<std::uint8_t[]> buf_;
    bool initialized_ = false;
    int compressionLevel_;
};

} // namespace clarisma

