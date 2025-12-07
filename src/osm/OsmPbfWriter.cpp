// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include "OsmPbfWriter.h"
#include <clarisma/util/Bytes.h>
#include <clarisma/util/varint.h>

OsmPbfWriter::OsmPbfWriter()
{
}

void OsmPbfWriter::writeNode(NodePtr node)
{
    for(;;)
    {
        if(encoder_.addNode(node)) [[likely]] return;
        flush();
    }
}

void OsmPbfWriter::writeNode(int64_t id, Coordinate xy)
{
    if(encoder_.addNode(id, xy)) [[likely]] return;
    flush();
    bool ok = encoder_.addNode(id, xy);
    assert(ok);
}

void OsmPbfWriter::writeOsmDataHeader(uint32_t compressedSize, uint32_t uncompressedSize)
{
    uint8_t buf[64];

    size_t dataSize = varintSize(compressedSize) + varintSize(uncompressedSize) +
        compressedSize + 2;
    uint32_t blobHeaderSize = Bytes::reverseByteOrder32(varintSize(dataSize) + 10);
    memcpy(&buf, &blobHeaderSize, 4);
    memcpy(&buf[4], "\x0A\x07OSMData\x18", 10);
    uint8_t* p = &buf[14];
    writeVarint(p, dataSize);
    assert(p == &buf[4] + blobHeaderSize);
    out_.writeAll(buf, blobHeaderSize + 4);
}

