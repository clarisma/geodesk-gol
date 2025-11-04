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
#include "UrlView.h"
#endif
#include "HttpRequestHeaders.h"
#include "SimpleUrlView.h"

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
    HttpResponse get(const char* url, const HttpRequestHeaders& headers = HttpRequestHeaders());
    std::string_view path() const { return path_; }
#else
    httplib::Client& client() { return client_; }
    std::string_view path() const { return urlView_.path(); }
#endif
    void get(const char* url, std::vector<std::byte>& data);

private:

#ifdef _WIN32
    static void closeAndThrow(HINTERNET& handle);

    HINTERNET hSession_ = nullptr;
    HINTERNET hConnect_ = nullptr;
    std::wstring host_;
    std::wstring userAgent_;
    int port_;
    bool useSSL_ = false;
#else
    SimpleUrlView urlView_;
    std::string origin_;
    httplib::Client client_;
    // bool useSSL_ = false;
#endif
    std::string path_;
};

} // namespace clarisma