// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <string>
#include <clarisma/util/DateTime.h>

struct OsmPbfMetadata
{
    std::string source;
    std::string replicationUrl;
    std::string generator;
    clarisma::DateTime replicationTimestamp;
    uint32_t replicationSequence;
};
