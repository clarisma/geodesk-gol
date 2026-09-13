// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <clarisma/net/HttpRequestHeaders.h>
#include <clarisma/text/Format.h>

namespace clarisma {

void HttpRequestHeaders::addRange(uint64_t start, uint64_t len)
{
    char buf[64];
    char* p = Format::unsignedIntegerReverse(start+len-1, buf+sizeof(buf));
    p--;
    *p = '-';
    p = Format::unsignedIntegerReverse(start, p);
    p -= 6;
    memcpy(p, "bytes=", 6);
    add("Range", {p, static_cast<size_t>(buf+sizeof(buf)-p)});
}

} // namespace clarisma