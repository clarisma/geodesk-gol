// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <httplib.h>
#include <clarisma/net/HttpException.h>
#include <clarisma/net/HttpRequestHeaders.h>

namespace clarisma {

template<typename Derived>
bool HttpResponseReader<Derived>::get(const char* url, const HttpRequestHeaders& reqHeaders)
{
    auto derived = self();
    uint32_t filled = 0;

    UrlView urlView(url);
    // TODO, this is dirty, path may not be 0-terminated
    url = "/"; // TODO urlView.path().data();

    auto res = derived->client()->client().Get(url,
        reqHeaders.asHttplibHeaders(),
        [&](const httplib::Response& response)
        {
            return derived->acceptResponse(response.status,
                HttpResponseHeaders(response.headers));
        },
        [&](const char* data, size_t dataLen)
        {
            auto src = reinterpret_cast<const std::byte*>(data);

            while (dataLen > 0)
            {
                uint32_t space = chunkSize_ - filled;
                uint32_t n = static_cast<uint32_t>(
                    std::min<size_t>(dataLen, space));

                std::memcpy(chunk_ + filled, src, n);
                filled += n;
                src += n;
                dataLen -= n;

                if (filled == chunkSize_)
                {
                    if (!(derived->*dispatcher_)())
                    {
                        return false;   // tell httplib to stop
                    }
                    filled = 0;
                }
            }
            return true;
        });

    if (!res)
    {
        httplib::Error error = res.error();
        if (error != httplib::Error::Canceled)
        {
            throw HttpException(res.error());
        }
    }
    return true;
}

} // namespace clarisma
