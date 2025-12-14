// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "OsmQueryPrinter.h"
#include <clarisma/io/File.h>
#include <clarisma/thread/TaskQueue.h>
#include <clarisma/zip/Deflater.h>
#include "osm/OsmPbfEncoder.h"

using namespace geodesk;

class OsmPbfQueryPrinter : public OsmQueryPrinter
{
public:
    explicit OsmPbfQueryPrinter(QuerySpec* spec);
    void processTask(const std::unique_ptr<const uint8_t[]>& block);

protected:
    void beginFeatures(int typeCode) override;
    void printNodes(std::span<SortedFeature> nodes) override;
    void printWays(std::span<SortedFeature> nodes) override;
    void printRelations(std::span<SortedFeature> nodes) override;
    void endFeatures() override;

private:
    void flush(int typeCode);
    void processOutput();
    void deflateMessageStart(int tagByte, uint32_t size);
    void deflateMessage(int tagByte, const uint8_t* p, uint32_t size);
    void deflatePrimitiveBlockStart(const uint8_t* pStringTable,
        uint32_t stringTableSize, uint32_t primitiveGroupSize);
    void writeOsmHeaderBlock();
    void writeOsmDataBlock(std::span<uint8_t> compressed, uint32_t uncompressedSize);
    static void encodeBlobHeader(uint8_t*& p, uint32_t dataSize, bool forHeader);
    static void encodeTinyString(uint8_t*& p, int tagByte, const std::string_view& s);

    OsmPbfEncoder encoder_;
    TaskQueue<OsmPbfQueryPrinter,std::unique_ptr<const uint8_t[]>> outputQueue_;
    FileHandle out_;
    // TODO: maybe add padding so we don't get false sharing; the following
    //  are used exclusively by the output thread
    Deflater deflater_;
    std::thread outputThread_;
};
