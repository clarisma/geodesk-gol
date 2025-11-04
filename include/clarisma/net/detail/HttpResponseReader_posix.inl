// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <httplib.h>
#include <clarisma/net/HttpException.h>
#include <clarisma/net/HttpRequestHeaders.h>

#include "clarisma/util/log.h"

namespace clarisma {

template<typename Derived>
bool HttpResponseReader<Derived>::get(const char* url, const HttpRequestHeaders& reqHeaders, bool readAll)
{
    auto derived = self();
    auto client = derived->client();
    uint32_t filled = 0;

    std::string path;
    if (url[0] == '/')
    {
        path = url;
    }
    else if (url[0] == 0)
    {
        path = client->path();
    }
    else
    {
        path = std::string(client->path()) + "/" + url;
    }

    LOGS << "Client is open: " << client->client().is_socket_open();

    auto res = client->client().Get(path,
        reqHeaders.asHttplibHeaders(),
        [&](const httplib::Response& response)
        {
            return derived->acceptResponse(response.status,
                HttpResponseHeaders(response.headers));
        },
        [&](const char* data, size_t dataLen)
        {
            if (!derived->dispatcher_) [[unlikely]]
            {
                return readAll;
            }

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
                        return readAll;
                        // return false;   // tell httplib to stop
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
    LOGS << "Client stayed open: " << client->client().is_socket_open();

    return true;
}

} // namespace clarisma
