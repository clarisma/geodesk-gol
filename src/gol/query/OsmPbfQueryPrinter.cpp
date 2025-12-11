// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include "OsmPbfQueryPrinter.h"
#include <clarisma/util/Bytes.h>
#include <clarisma/util/varint.h>

#include "OsmQueryPrinter.h"

OsmPbfQueryPrinter::OsmPbfQueryPrinter(QuerySpec* spec) :
    OsmQueryPrinter(spec),
    encoder_(spec->store(), spec->keys()),
    out_(Console::get()->handle(Console::Stream::STDOUT)),
    outputQueue_(4), // TODO
    deflater_(OsmPbfEncoder::BLOCK_SIZE),    // TODO: accounts for overhead?
    outputThread_(&OsmPbfQueryPrinter::processOutput, this)
{
    // TODO
}


void OsmPbfQueryPrinter::beginFeatures(int typeCode)
{
    encoder_.start(OsmPbfEncoder::GroupCode::fromTypeCode(typeCode));
}

void OsmPbfQueryPrinter::printNodes(std::span<SortedFeature> nodes)
{
    for (auto& node : nodes)
    {
        if (node.data.isCoordinate())
        {
            for (;;)
            {
                if(encoder_.addNode(node.id, node.data.lon(), node.data.lat())) [[likely]] return;
                flush();
            }
        }
        for (;;)
        {
            if(encoder_.addNode(node.data.node())) [[likely]] return;
            flush();
        }
    }
}


void OsmPbfQueryPrinter::printWays(std::span<SortedFeature> ways)
{
    for (auto& way : ways)
    {
        for (;;)
        {
            if(encoder_.addWay(way.data.way())) [[likely]] return;
            flush();
        }
    }
}


void OsmPbfQueryPrinter::printRelations(std::span<SortedFeature> rels)
{
    for (auto& rel : rels)
    {
        for (;;)
        {
            if(encoder_.addRelation(rel.data.relation())) [[likely]] return;
            flush();
        }
    }
}


void OsmPbfQueryPrinter::flush()
{
    outputQueue_.post(encoder_.takeBlock());
}

void OsmPbfQueryPrinter::endFeatures()
{
    if (!encoder_.isEmpty()) flush();
    outputQueue_.awaitCompletion();
    outputQueue_.shutdown();
}

void OsmPbfQueryPrinter::processOutput()
{
    // TODO: write OSMHeader
    outputQueue_.process(this);
}


void OsmPbfQueryPrinter::processTask(std::unique_ptr<const uint8_t[]> block)
{
    const OsmPbfEncoder::Manifest* manifest =
        reinterpret_cast<const OsmPbfEncoder::Manifest*>(block.get());

    uint32_t stringTableSize = manifest->stringsSize;
    // uint32_t primitiveBlockSize = stringTableSize + varintSize(stringTableSize) + 1;
    if (manifest->groupCode == OsmPbfEncoder::GroupCode::NODES)
    {
        uint32_t idsSize = manifest->featuresSize;
        uint32_t latsSize = manifest->nodeLatsSize;
        uint32_t lonsSize = manifest->nodeLonsSize;
        uint32_t tagsSize = manifest->nodeTagsSize;
        uint32_t denseNodesSize = idsSize + varintSize(idsSize) +
            latsSize + varintSize(latsSize) +
            lonsSize + varintSize(lonsSize) + 3;
        if (tagsSize)
        {
            denseNodesSize += tagsSize + varintSize(tagsSize) + 1;
        }
        // primitiveBlockSize += groupSize + varintSize(groupSize) + 1;

        deflatePrimitiveBlockStart(manifest->pStrings, stringTableSize,
            denseNodesSize + varintSize(denseNodesSize) + 1);
        deflateMessageStart(OsmPbf::GROUP_DENSENODES, denseNodesSize);
        deflateMessage(OsmPbf::DENSENODE_IDS, manifest->pFeatures, idsSize);
        deflateMessage(OsmPbf::DENSENODE_LATS, manifest->pNodeLats, latsSize);
        deflateMessage(OsmPbf::DENSENODE_LONS, manifest->pNodeLons, lonsSize);
        if (tagsSize)
        {
            deflateMessage(OsmPbf::DENSENODE_TAGS, manifest->pNodeTags, tagsSize);
        }
    }
    else
    {
        uint32_t featuresSize = manifest->featuresSize;
        deflatePrimitiveBlockStart(manifest->pStrings, stringTableSize,
            featuresSize + varintSize(featuresSize) + 1);
        deflater_.deflate(manifest->pFeatures, featuresSize);
    }
    deflater_.finish();

    std::span<uint8_t> compressed = deflater_.deflated();
    uint32_t rawSize = static_cast<uint32_t>(deflater_.uncompressedSize());
    uint32_t compressedSize = static_cast<uint32_t>(compressed.size());
    uint32_t blobSize =
        rawSize + varintSize(rawSize) + 1 +
        compressedSize + varintSize(compressedSize) + 1;


    // TODO: allow reset of Deflater

}

void OsmPbfQueryPrinter::deflatePrimitiveBlockStart(const uint8_t* pStringTable,
    uint32_t stringTableSize, uint32_t primitiveGroupSize)
{
    deflateMessage(OsmPbf::BLOCK_STRINGTABLE, pStringTable, stringTableSize);
    deflateMessageStart(OsmPbf::BLOCK_GROUP, primitiveGroupSize);
}


void OsmPbfQueryPrinter::deflateMessage(int typeByte, const uint8_t* p, uint32_t size)
{
    deflateMessageStart(typeByte, size);
    deflater_.deflate(p, size);   // NOLINT no escape
}

void OsmPbfQueryPrinter::deflateMessageStart(int typeByte, uint32_t size)
{
    uint8_t buf[16];
    buf[0] = typeByte;
    uint8_t* p = &buf[1];
    writeVarint(p, size);
    deflater_.deflate(&buf, p - buf);               // NOLINT no escape
}

// Each blob has the following format:
// 4   length of the BlobHeader message in network byte order
// 1   STRING #1
// 1   7 (length of string)
// 7   "OSMData"
// 1   VARINT #3
// n   size of data that follows
// 1   VARINT #2
// n   uncompressed block size
// 1   BYTES #3

// TODO

void OsmPbfQueryPrinter::writeOsmDataHeader(uint32_t compressedSize, uint32_t uncompressedSize)
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

