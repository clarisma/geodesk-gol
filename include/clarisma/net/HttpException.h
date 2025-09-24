// Copyright (c) 2024 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#include <clarisma/io/IOException.h>
#ifndef _WIN32
#include <httplib.h>
#endif

namespace clarisma {

class HttpException : public IOException
{
public:
    using IOException::IOException;

#ifdef _WIN32
    explicit HttpException(unsigned long errorCode);
#else
    explicit HttpException(httplib::Error errorCode);
#endif
};



} // namespace clarisma
