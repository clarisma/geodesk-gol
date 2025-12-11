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

protected:
    void beginFeatures(int typeCode) override;
    void printNodes(std::span<SortedFeature> nodes) override;
    void printWays(std::span<SortedFeature> nodes) override;
    void printRelations(std::span<SortedFeature> nodes) override;
    void endFeatures() override;

private:
    void flush();
    void processOutput();
    void writeOsmDataHeader(uint32_t compressedSize, uint32_t uncompressedSize);

    OsmPbfEncoder encoder_;
    TaskQueue<OsmPbfQueryPrinter,std::unique_ptr<uint8_t[]>> queue_;
    std::thread outputThread_;
    Deflater deflater_;
    File out_;
    std::unique_ptr<uint8_t[]> buf_;
};
