// Copyright (c) 2024 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#ifdef CLARISMA_WITH_ZLIB

#pragma once
#include <memory>
#include <clarisma/io/File.h>

using std::byte;

namespace clarisma {

class Inflater
{
public:
	explicit Inflater(uint32_t bufferSize = 256 * 1024);

	/// @brief Decompresses a raw deflate-compressed block from a file
	/// into a provided buffer, and verifies the CRC32C checksum
	/// of the decompressed data. The size of the compressed and
	/// uncompressed data must be less than 4 GB.
	///
	/// @param file     the file handle
	/// @param ofs      offset at which the raw zlib data is stored
	/// @param srcSize  the size of the compressed data
	/// @param dest     the buffer where the uncompressed data will be written
	/// @param destSize the size of the uncompressed data
	/// @param checksum the CRC32C of the uncompressed data
	///
	void inflateRaw(FileHandle file, uint64_t ofs, uint32_t srcSize,
		byte* dest, uint32_t destSize, uint32_t checksum);

private:
	std::unique_ptr<byte[]> buffer_;
	uint32_t bufferSize_;
};

} // namespace clarisma

#endif