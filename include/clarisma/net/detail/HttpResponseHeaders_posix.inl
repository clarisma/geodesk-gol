// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <clarisma/text/Parsing.h>

namespace clarisma {

inline int HttpResponseHeaders::status() const
{

}

inline size_t HttpResponseHeaders::contentLength() const
{
    auto it = headers_.find("Content-Length");
    if (it == headers_.end()) return 0;
    return Parsing::parseUnsignedLong(it->second);
}


inline std::string HttpResponseHeaders::etag() const
{
    auto it = headers_.find("ETag");
    if (it == headers_.end()) return {};
    return it->second;
}

} // namespace clarisma
