// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <clarisma/net/HttpClient.h>
#include <clarisma/net/HttpException.h>
#include <clarisma/io/IOException.h>
#include <clarisma/net/UrlView.h>
#include <clarisma/util/log.h>
#include <clarisma/util/Strings.h>

namespace clarisma {

void HttpClient::closeAndThrow(HINTERNET& handle)
{
    DWORD error = GetLastError();
    WinHttpCloseHandle(handle);
    handle = nullptr;
    LOGS << "HttpClient::closeAndThrow throws IOException for error "
        << static_cast<int>(error);
    throw HttpException(error);
}

std::wstring HttpClient::toWideString(std::string_view s)
{
    int size = static_cast<int>(s.size());
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, s.data(), size, NULL, 0);
    std::wstring wideString(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), size, &wideString[0], sizeNeeded);
    return wideString;
}

HttpClient::HttpClient(std::string_view url)
{
    UrlView uv(url);
    host_ = toWideString(uv.host());
    port_ = uv.port();
    if (uv.scheme() == "https")
    {
        useSSL_ = TRUE;
    }
    else if (uv.scheme() != "http")
    {
        throw IOException(Strings::combine("Unsupported scheme: ", uv.scheme()));
    }
    std::string_view path = uv.path();
    if (!path.empty())
    {
        if (path.back() == '/') path.remove_suffix(1);
        path_ = path;
    }
}

HttpClient::~HttpClient()
{
    close();
}

void HttpClient::open()
{
    hSession_ = WinHttpOpen(userAgent_.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession_) throw HttpException(GetLastError());

    hConnect_ = WinHttpConnect(hSession_, host_.c_str(), port_, 0);
    if (!hConnect_) closeAndThrow(hSession_);
}

HttpResponse HttpClient::get(const char* url)
{
    if(!isOpen()) open();

    std::wstring urlW;
    if (url[0] == '/')
    {
        urlW = toWideString(url);
    }
    else
    {
        // TODO: this is inefficent
        urlW = toWideString(path_) + L'/' + toWideString(url);
    }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect_, L"GET", urlW.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        useSSL_ ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) throw HttpException(GetLastError());

    // Send the request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
        0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
    {
        closeAndThrow(hRequest);
    }

    // Receive the response
    if (!WinHttpReceiveResponse(hRequest, NULL))
    {
        closeAndThrow(hRequest);
    }
    return HttpResponse(hRequest);
}

void HttpClient::close()
{
    if(hConnect_)
    {
        WinHttpCloseHandle(hConnect_);
        hConnect_ = nullptr;
    }
    if(hSession_)
    {
        WinHttpCloseHandle(hSession_);
        hSession_ = nullptr;
    }
}


void HttpClient::get(const char* url, std::vector<std::byte>& data)
{
    HttpResponse response = get(url);
    int status = response.status();
    if (status != 200)
    {
        throw HttpException("Server response %d", status);
    }
    response.read(data);
}

} // namespace clarisma