// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include "OsmPbfQueryPrinter.h"
#include <clarisma/util/Bytes.h>
#include <clarisma/util/varint.h>

#include "OsmQueryPrinter.h"

OsmPbfQueryPrinter::OsmPbfQueryPrinter(QuerySpec* spec) :
    OsmQueryPrinter(spec),
    encoder_(spec->store(), spec->keys(), false),   // TODO: flag for locationOnWays
    out_(Console::handle(Console::Stream::STDOUT)),
    outputQueue_(4), // TODO
    deflater_(OsmPbfEncoder::BLOCK_SIZE),    // TODO: accounts for overhead?
    outputThread_(&OsmPbfQueryPrinter::processOutput, this)
{
    // TODO
}


void OsmPbfQueryPrinter::beginFeatures(int typeCode)
{
    auto prevBlock = encoder_.start(OsmPbfEncoder::GroupCode::fromTypeCode(typeCode));
    if (prevBlock) outputQueue_.post(std::move(prevBlock));
}

void OsmPbfQueryPrinter::printNodes(std::span<SortedFeature> nodes)
{
    for (auto& node : nodes)
    {
        if (node.data.isCoordinate())
        {
            for (;;)
            {
                if(encoder_.addNode(node.id, node.data.lon(), node.data.lat())) [[likely]] break;
                flush();
            }
        }
        for (;;)
        {
            if(encoder_.addNode(node.data.node())) [[likely]] break;
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
            if(encoder_.addWay(way.data.way())) [[likely]] break;
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
            if(encoder_.addRelation(rel.data.relation())) [[likely]] break;
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
    LOGS << "Waiting for writer output thread to finish...";
    outputQueue_.awaitCompletion();
    LOGS << "Shutting down output";
    outputQueue_.shutdown();
    if (outputThread_.joinable()) outputThread_.join();
}

void OsmPbfQueryPrinter::processOutput()
{
    writeOsmHeaderBlock();
    outputQueue_.process(this);
}


void OsmPbfQueryPrinter::processTask(const std::unique_ptr<const uint8_t[]>& block)
{
    const OsmPbfEncoder::Manifest* manifest =
        reinterpret_cast<const OsmPbfEncoder::Manifest*>(block.get());

    deflater_.begin();
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
    LOGS << "Writing " << compressed.size() << " compressed bytes ("
        << rawSize << " bytes raw)";
    writeOsmDataBlock(compressed, rawSize);
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


// Header has the following format:
// 4   length of the BlobHeader message in network byte order
// 1   STRING #1
// 1   9 (length of string)
// 9   "OSMHeader"
// 1   VARINT #3
// n   size of data that follows
// 1   VARINT #1
// n   raw data
// One or more:
//    1   STRING #4  (required feature)
//    n   stringLen
//    n   stringData
// Zero or more:
//    1   STRING #5  (optional feature)
//    n   stringLen
//    n   stringData

void OsmPbfQueryPrinter::writeOsmHeaderBlock()
{
    uint8_t headerData[1024];
    uint8_t* p = headerData;
    // TODO: bbox
    encodeTinyString(p, HEADER_REQUIRED_FEATURES, "OsmSchema-V0.6");
    encodeTinyString(p, HEADER_REQUIRED_FEATURES, "DenseNodes");
    encodeTinyString(p, HEADER_OPTIONAL_FEATURES, "Sort.Type_then_ID");
    // encodeTinyString(p, HEADER_OPTIONAL_FEATURES, "LocationsOnWays");
    // TODO
    encodeTinyString(p, HEADER_WRITINGPROGRAM, "gol/" GEODESK_GOL_VERSION);

    size_t headerDataSize = p - headerData;
    assert(headerDataSize <= sizeof(headerData));

    uint8_t buf[1024];
    p = buf;
    encodeBlobHeader(p, headerDataSize, true);
    memcpy(p, headerData, headerDataSize);
    p += headerDataSize;
    assert(p - buf <= sizeof(buf));
    out_.writeAll(buf, p - buf);
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
// n   length of zlib-compressed data
// ---------- zlib-compressed data -------------

void OsmPbfQueryPrinter::writeOsmDataBlock(std::span<uint8_t> compressed, uint32_t uncompressedSize)
{
    uint8_t buf[64];
    size_t compressedSize = compressed.size();
    size_t dataSize = varintSize(compressedSize) + varintSize(uncompressedSize) +
        compressedSize + 2;
    uint8_t* p = buf;
    encodeBlobHeader(p, dataSize, false);
    *p++ = OsmPbf::BLOB_RAW_SIZE;
    writeVarint(p, uncompressedSize);
    *p++ = OsmPbf::BLOB_ZLIB_DATA;
    writeVarint(p, compressedSize);
    out_.writeAll(buf, p - buf);
    out_.writeAll(compressed.data(), compressedSize);
}


void OsmPbfQueryPrinter::encodeTinyString(uint8_t*& p, int tagByte, const std::string_view& s)
{
    assert(s.size() < 128);
    *p++ = tagByte;
    *p++ = s.size();
    memcpy(p, s.data(), s.size());
    p += s.size();
}


void OsmPbfQueryPrinter::encodeBlobHeader(uint8_t*& p, uint32_t dataSize, bool forHeader)
{
    uint8_t* buf = p;
    uint32_t headerPreludeSize = forHeader ? 12 : 10;
    uint32_t blobHeaderSize = varintSize(dataSize) + headerPreludeSize;
    uint32_t blobHeaderSizeBigEndian = Bytes::reverseByteOrder32(blobHeaderSize);
    memcpy(p, &blobHeaderSizeBigEndian, 4);
    p += 4;
    memcpy(p, forHeader ? "\x0A\x07OSMHeader\x18" : "\x0A\x07OSMData\x18",
        headerPreludeSize);
    p += headerPreludeSize;
    writeVarint(p, dataSize);
    assert(p == &buf[4] + blobHeaderSize);
}