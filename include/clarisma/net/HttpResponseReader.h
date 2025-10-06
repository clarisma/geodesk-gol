// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#include <clarisma/net/HttpClient.h>
#include <clarisma/net/HttpException.h>
#include <clarisma/net/HttpHeaders.h>

namespace clarisma {

class HttpHeaders;

template<typename Derived>
class HttpResponseReader
{
public:
    HttpResponseReader(HttpClient& client) :
        client_(client) {}

    bool get(const char* url);

    using Dispatcher = void (Derived::*)();

    bool acceptHeaders(const HttpHeaders& headers)  // CRTP virtual
    {
        return true;
    }

private:
    Derived* self() { return reinterpret_cast<Derived*>(this); }

    HttpClient& client_;
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