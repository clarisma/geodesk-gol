// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#ifndef NOMINMAX
// Prevent Windows headers from clobbering min/max
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <clarisma/net/HttpRequestHeaders.h>

namespace clarisma {

template<typename Derived>
bool HttpResponseReader<Derived>::get(const char* url, const HttpRequestHeaders& reqHeaders)
{
    HttpResponse response = self()->client()->get(url, reqHeaders);
    HINTERNET request = response.handle();
    HttpResponseHeaders headers(request);
    if (!self()->acceptHeaders(headers)) return false;

    do
    {
        std::byte* p = chunk_;
        uint32_t remaining = chunkSize_;
        do
        {
            DWORD bytesRead = 0;
            if (!WinHttpReadData(request, p, remaining, &bytesRead))
            {
                throw HttpException(GetLastError());
            }
            p += bytesRead;
            remaining -= bytesRead;
        }
        while (remaining > 0);
    }
    while ((self()->*dispatcher_)());
    return true;
}

} // namespace clarisma
