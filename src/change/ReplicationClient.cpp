// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ReplicationClient.h"
#include <cmath>
#include <clarisma/math/Decimal.h>
#include <clarisma/net/HttpException.h>
#include <clarisma/text/Format.h>
#include <clarisma/util/log.h>
#include <clarisma/util/PropertiesParser.h>
#include <clarisma/util/Strings.h>

// TODO: state file may be incompletely written? Retry reading
ReplicationClient::State ReplicationClient::fetchState(const char* path)
{
    std::vector<std::byte> data;
    get(path, data);

    State state;
    std::string_view properties(
        reinterpret_cast<const char *>(data.data()),data.size());

    PropertiesParser parser(properties);
    std::string_view key, value;
    while(parser.next(key, value))
    {
        if(key == "timestamp")
        {
            char buf[64];
            int len = std::min(value.size(), sizeof(buf)-1);
            memcpy(buf, value.data(), len);
            buf[len] = 0;
            Strings::removeChar(buf, '\\');
            state.timestamp = DateTime(buf,"%Y-%m-%dT%H:%M:%SZ");
        }
        else if (key == "sequenceNumber")
        {
            state.revision = static_cast<uint32_t>(Decimal(value));
        }
    }
    return state;
}

char* ReplicationClient::formatRevisionPath(char* buf, uint32_t revision)
{
    char* p = Format::unsignedZeroFilled(buf, revision / 1'000'000, 3);
    *p++ = '/';
    p = Format::unsignedZeroFilled(p, revision / 1000, 3);
    *p++ = '/';
    p = Format::unsignedZeroFilled(p, revision, 3);
    *p = 0;
    return p;
}

void ReplicationClient::fetch(uint32_t revision, std::vector<std::byte>& data)
{
    char path[32];
    char* p = formatRevisionPath(path, revision);
    strcpy(p, ".osc.gz");   // NOLINT safe

    // TODO: Implement for Linux; disabled for now

    /*
    HttpResponse response = get(path);
    int status = response.status();
    if (status != 200) [[unlikely]]
    {
        if (status == 404)
        {
            throw HttpException("Revision %d is missing on server", revision);
        }
        throw HttpException("Server returned error %d", status);
    }
    response.readUnzippedGzip(data);
    */
}

ReplicationClient::State ReplicationClient::fetchState(uint32_t revision)
{
    LOGS << "Fetching state for revision " << revision;
    char path[32];
    char* p = formatRevisionPath(path, revision);
    strcpy(p, ".state.txt");    // NOLINT safe
    return fetchState(path);
}

/// The following function is adapted from PyOsmium
/// by Sarah Hoffman and others.
/// https://github.com/osmcode/pyosmium/blob/master/src/osmium/replication/server.py
///
/// The original work is licensed as follows:
/// Copyright (C) 2023 Sarah Hoffmann <lonvia@denofr.de> and others.
/// Licensed under BSD 2-Clause
///
/// Changes by GeoDesk Contributors licensed under AGPL 3.0
///
ReplicationClient::State ReplicationClient::findCurrentState(
    DateTime timestamp, State upper)
{
    LOGS << "Finding revision for " << timestamp;

    if (upper.timestamp < timestamp)    [[unlikely]]
    {
        return upper;
    }
    State lower;
    int revisionLower = upper.revision / 2;
    for (;;)
    {
        try
        {
            lower = fetchState(revisionLower);
        }
        catch (IOException& ex)
        {
            // TODO: Ideally, we should only catch 404
            //  and propagate true failures (e.g. connection lost)
            int revisionSplit = (revisionLower + upper.revision) / 2;
            if (revisionSplit == revisionLower) return upper;
            revisionLower = revisionSplit;
            continue;
        }
        LOGS << "  Lower: " << lower;
        LOGS << "  Upper: " << upper;
        if (lower.timestamp < timestamp) break;
        upper = lower;
        revisionLower = 0;
    }

    /*
    LOGS << "Searching between " << lower.revision << " (" << lower.timestamp
        << ") and " << upper.revision << " (" << upper.timestamp << ")";
    */


    for (;;)
    {
        LOGS << "Searching between " << lower << " and " << upper;

        int64_t timeInterval = (upper.timestamp - lower.timestamp) / 1000;
        int64_t revInterval = upper.revision - lower.revision;
        int64_t goal = (timestamp - lower.timestamp) / 1000;
        uint32_t revisionSplit = lower.revision + static_cast<uint32_t>(
            std::ceil(static_cast<double>(goal) * revInterval / timeInterval));
        if (revisionSplit >= upper.revision) [[unlikely]]
        {
            revisionSplit = upper.revision - 1;
        }
        State split = fetchState(revisionSplit);
            // TODO: Do we need to handle missing sequences in the middle?
        LOGS << "  Splitting at " << split;
        if (split.timestamp < timestamp)
        {
            lower = split;
        }
        else
        {
            upper = split;
        }
        if (lower.revision + 1 >= upper.revision)
        {
            return lower;
        }
    }
}