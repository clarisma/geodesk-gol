// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <clarisma/net/HttpException.h>

namespace clarisma {

#ifdef _WIN32

HttpException::HttpException(unsigned long errorCode) :
    IOException(getMessage("winhttp.dll", errorCode))
{
}

#else

HttpException::HttpException(httplib::Error errorCode) :
    IOException(httplib::to_string(errorCode))
{
}

#endif

} // namespace clarisma