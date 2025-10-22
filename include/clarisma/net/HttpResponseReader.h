// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#include <clarisma/net/HttpClient.h>
#include <clarisma/net/HttpException.h>
#include <clarisma/net/HttpResponseHeaders.h>

namespace clarisma {

class HttpRequestHeaders;
class HttpResponseHeaders;

template<typename Derived>
class HttpResponseReader
{
public:
    bool get(const char* url, const HttpRequestHeaders& headers);

    using Dispatcher = bool (Derived::*)();

    bool acceptHeaders(const HttpResponseHeaders& headers)  // CRTP virtual
    {
        return true;
    }

    // HttpClient* client(); // CRTP override
protected:
    void receive(std::byte* data, size_t size, Dispatcher dp)
    {
        chunk_ = data;
        assert(size < (1ULL << 32));
        chunkSize_ = static_cast<uint32_t>(size);
        dispatcher_ = dp;
    }

private:
    Derived* self() { return reinterpret_cast<Derived*>(this); }

    std::byte* chunk_ = nullptr;
    uint32_t chunkSize_ = 0;
    Dispatcher dispatcher_ = nullptr;
};

} // namespace clarisma

#if defined(_WIN32)
#include "detail/HttpResponseReader_win.inl"
#else
#include "detail/HttpResponseReader_posix.inl"
#endif