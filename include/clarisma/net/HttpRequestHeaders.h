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

class HttpRequestHeaders
{
public:
    void add(const std::string_view& key, const std::string_view& value);
    void addRange(uint64_t start, uint64_t len);

#ifdef _WIN32
    bool isEmpty() const noexcept { return headers_.empty(); }
    std::string_view asStringView() const noexcept { return headers_; }

    std::string headers_;
#else
    const httplib::Headers& headers_;
#endif
};

} // namespace clarisma

#if defined(_WIN32)
#include "detail/HttpRequestHeaders_win.inl"
#else
#include "detail/HttpRequestHeaders_posix.inl"
#endif