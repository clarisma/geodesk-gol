// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#ifdef CLARISMA_WITH_ZLIB

#include <clarisma/zip/Zip.h>
#include <clarisma/util/DataPtr.h>

namespace clarisma::Zip {

void getGzipMetadata(const uint8_t* data, size_t size, GzipMetadata* meta)
{
    const uint8_t* pEnd = data + size;
    enum Flags
    {
        TEXT = 1,
        CRC16 = 2,
        EXTRA = 4,
        NAME = 8,
        COMMENT = 16
    };
    DataPtr p(data);
    if(p.getUnsignedShort() != 0x8b1f)
    {
        throw ZipException("Not a valid gzip file");
    }
    int flags = (p+3).getByte();
    meta->timestamp = (p+4).getInt();
    p += 10;
    if(flags & EXTRA)
    {
        meta->extraDataSize = p.getUnsignedShort();
        p += 2;
        meta->extraData = p;
        p += meta->extraDataSize;
    }
    if(flags & NAME)
    {
        meta->fileName = p.charPtr();
        while(p.ptr() < pEnd)
        {
            uint8_t ch = p.getByte();
            p += 1;
            if(ch == 0) break;
        }
    }
    if(flags & COMMENT)
    {
        meta->comment = p.charPtr();
        while(p.ptr() < pEnd)
        {
            uint8_t ch = p.getByte();
            p += 1;
            if(ch == 0) break;
        }
    }
    if(flags & CRC16) p += 2;
    if(pEnd - p.ptr() < 8)
    {
        throw ZipException("Invalid gzip file");
    }
    meta->compressedData = p.ptr();
    p = pEnd - 8;
    meta->checksum = p.getUnsignedInt();
    meta->uncompressedSize = (p+4).getUnsignedInt();
}

ByteBlock inflateGzip(const uint8_t* data, size_t size)
{
    GzipMetadata meta;
    getGzipMetadata(data, size, &meta);
    size_t headerSize = meta.compressedData - data;
    size_t compressedSize = size - headerSize - 8; // 8-byte footer
    return inflateRaw(meta.compressedData, compressedSize, meta.uncompressedSize);
}

} // namespace clarisma::Zip

#endif