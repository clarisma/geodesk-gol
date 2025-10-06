// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <clarisma/text/Parsing.h>

namespace clarisma {

inline int HttpHeaders::status() const
{
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);

    // Query the status code from the headers
    if (!WinHttpQueryHeaders(hRequest_,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
        &statusCodeSize, WINHTTP_NO_HEADER_INDEX))
    {
        // TODO: Check for errors other than header not present
        // std::cerr << "Error querying status code: " << GetLastError() << std::endl;
        return 0;
    }
    return static_cast<int>(statusCode);
}

inline size_t HttpHeaders::contentLength() const
{
    wchar_t buf[32];
    DWORD bufSize = sizeof(buf);
    if (!WinHttpQueryHeaders(hRequest_,
        WINHTTP_QUERY_CONTENT_LENGTH,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &buf,
        &bufSize,
        WINHTTP_NO_HEADER_INDEX))
    {
        // TODO: Check for errors other than header not present
        // std::cerr << "Failed to get ContentLength" << std::endl;
        return 0;
    }
    return Parsing::parseUnsignedLong(buf);
}

} // namespace clarisma
