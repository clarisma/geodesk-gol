// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <clarisma/io/File.h>
#include <clarisma/thread/TaskQueue.h>
#include <clarisma/zip/Deflater.h>
#include "OsmPbfEncoder.h"

using namespace clarisma;

class OsmPbfWriter
{
public:
    void beginNodes();
    void beginWays();
    void beginRelations();
    void writeNode(NodePtr node);
    void writeNode(int64_t id, Coordinate xy);
    void writeWay(WayPtr way);
    void writeRelation(RelationPtr rel);

private:
    struct EncodedData
    {
        std::unique_ptr<uint8_t[]> data;
        uint32_t stringsSize;
        uint32_t featuresSize;
        uint32_t nodeIdsSize;
        uint32_t nodeLonsSize;
        uint32_t nodeLatsSize;
        uint32_t nodTagsSize;
    };

    void flush();
    void processOutput();
    void writeOsmDataHeader(uint32_t compressedSize, uint32_t uncompressedSize);

    OsmPbfEncoder encoder_;
    TaskQueue<OsmPbfWriter,EncodedData> queue_;
    std::thread outputThread_;
    Deflater deflater_;
    File out_;
    std::unique_ptr<uint8_t[]> buf_;
};
