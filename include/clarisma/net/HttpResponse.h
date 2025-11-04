// Copyright (c) 2024 Clarisma / GeoDesk contributors
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

class HttpResponse
{
public:
    HttpResponse() : hRequest_(nullptr) {}
#ifdef _WIN32
    explicit HttpResponse(const HINTERNET hRequest) : hRequest_(hRequest) {}
#else
    explicit HttpResponse(std::shared_ptr<httplib::Response> response) :
        response_(std::move(response)) {}
#endif
    ~HttpResponse() { close(); }


    HttpResponse(const HttpResponse&) = delete;
    HttpResponse& operator=(const HttpResponse&) = delete;
    HINTERNET handle() const { return hRequest_; }

    int status() const;
    size_t contentLength() const;
    size_t read(void* buf, size_t size);
    void read(std::vector<std::byte>& data);
    void readUnzippedGzip(std::vector<std::byte>& data);
    void close();

private:
#ifdef _WIN32
    HINTERNET hRequest_;
#else
    std::shared_ptr<httplib::Response> response_;
#endif
};

} // namespace clarisma