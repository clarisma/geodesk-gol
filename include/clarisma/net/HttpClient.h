// Copyright (c) 2024 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#ifdef _WIN32
#include "HttpResponse.h"
#else
#include <string>
#include <string_view>
#include <vector>
#include <httplib.h>
#endif

namespace clarisma {

class HttpClient
{
public:
    explicit HttpClient(std::string_view url);
    ~HttpClient();
    void setUserAgent(const char* name);
    void setRedirects(int max);
    void setTimeout(int ms);
    void open();
    void close();
#ifdef _WIN32
    bool isOpen() const { return hConnect_ != nullptr; }
    HttpResponse get(const char* url);
#endif
    void get(const char* url, std::vector<std::byte>& data);

private:
    static std::wstring toWideString(std::string_view s);

#ifdef _WIN32
    static void closeAndThrow(HINTERNET& handle);

    HINTERNET hSession_ = nullptr;
    HINTERNET hConnect_ = nullptr;
    std::wstring host_;
    std::wstring userAgent_;
    int port_;
    bool useSSL_ = false;
#else
    union Clients
    {
        httplib::Client http;
        httplib::SSLClient ssl;

        Clients() {}  // Does nothing, objects constructed manually
        ~Clients() {} // Destructor must be manually called
    } client_;
    bool useSSL_ = false;
#endif
    std::string path_;
};

} // namespace clarisma