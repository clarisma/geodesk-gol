// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/net/HttpClient.h>
#include <clarisma/util/DateTime.h>

using namespace clarisma;

class Replication
{
public:
    struct State
    {
        uint32_t revision = 0;
        DateTime timestamp;
    };

    static State fetchState(HttpClient& client, const char* path = "state.txt");
    static State fetchState(HttpClient& client, uint32_t revision);
    static State findCurrentState(HttpClient& client, DateTime timestamp, State upper);

    static char* formatPath(char* buf, uint32_t revision);
};


template<typename Stream>
Stream& operator<<(Stream& out, Replication::State state)
{
    out << state.revision << " (" << state.timestamp << ")";
    return static_cast<Stream&>(out);
}
