// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <httplib.h>

namespace clarisma {

template<typename Derived>
bool HttpResponseReader<Derived>::get(const char* url, const HttpRequestHeaders& reqHeaders)
{
    auto derived = self();
    uint32_t filled = 0;

    auto res = derived->client()->Get(url, reqHeaders,
        [&](const httplib::Response& response)
        {
            return derived->acceptResponse(response.status, response.headers);
        },
        [&](const char* data, size_t dataLen)
        {
            const std::byte* src =
                reinterpret_cast<const std::byte*>(data);

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
                    if (!(d->*dispatcher_)())
                    {
                        return false;   // tell httplib to stop
                    }
                    filled = 0;
                }
            }
        });

    return res;
}

} // namespace clarisma
