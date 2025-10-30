// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
// Prevent Windows headers from clobbering min/max
#define NOMINMAX
#include <windows.h>
#endif
#include <winhttp.h>
#else
#include <httplib.h>        // use cpp-httplib for non-Windows
#endif
#include <string>
#include <vector>

namespace clarisma {

class HttpResponseHeaders
{
public:
#ifdef _WIN32
    explicit HttpResponseHeaders(const HINTERNET hRequest) : hRequest_(hRequest) {}
#else
    explicit HttpResponseHeaders(const httplib::Headers& headers) :
        headers_(headers) {}
#endif

    size_t contentLength() const;
    std::string etag() const;

private:
#ifdef _WIN32
    HINTERNET hRequest_;
#else
    const httplib::Headers& headers_;
#endif
};

} // namespace clarisma

#if defined(_WIN32)
#include "detail/HttpResponseHeaders_win.inl"
#else
#include "detail/HttpResponseHeaders_posix.inl"
#endif