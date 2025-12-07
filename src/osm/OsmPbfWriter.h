// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <clarisma/io/File.h>

using namespace clarisma;

class OsmPbfWriter
{
public:

private:
    struct PbfData
    {
        std::unique_ptr<uint_8[]> data;
        uint32_t stringsSize;
        uint32_t featuresSize;
        uint32_t nodeIdsSize;
        uint32_t nodeLonsSize;
        uint32_t nodeLatsSize;
        uint32_t nodTagsSize;
    };

    void writeOsmDataHeader(uint32_t compressedSize, uint32_t uncompressedSize);

    File out_;
    std::unique_ptr<uint8_t[]> buf_;
};
