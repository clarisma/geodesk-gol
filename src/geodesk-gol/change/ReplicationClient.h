// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/net/HttpClient.h>
#include <clarisma/util/DateTime.h>

using namespace clarisma;

class ReplicationClient : protected HttpClient
{
public:
    explicit ReplicationClient(std::string_view url) : HttpClient(url) {}

    struct State
    {
        uint32_t revision = 0;
        DateTime timestamp;
    };

    State fetchState(const char* path = "state.txt");
    State fetchState(uint32_t revision);
    void fetch(uint32_t revision, std::vector<std::byte>& data);
    State findCurrentState(DateTime timestamp, State upper);

private:
    static char* formatRevisionPath(char* buf, uint32_t revision);
};


template<typename Stream>
Stream& operator<<(Stream& out, ReplicationClient::State state)
{
    out << state.revision << " (" << state.timestamp << ")";
    return static_cast<Stream&>(out);
}
