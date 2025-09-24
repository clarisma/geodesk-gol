// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <zlib.h>
#include <clarisma/zip/Zip.h>
#include "TesException.h"
#include "TesParcel.h"

using namespace clarisma;

ByteBlock TesParcel::uncompress()
{
    ByteBlock block = Zip::inflate(data(), size(), sizeUncompressed_);
    Zip::verifyChecksum(block, checksum_);
    /*
    // Calculate the CRC32 checksum of the uncompressed data
    uint32_t calculatedChecksum = crc32_z(0, block.data(), static_cast<z_size_t>(block.size()));
    if (calculatedChecksum != checksum_)
    {
        throw TesException("Checksum mismatch");
    }
    */
    return block;
}
 