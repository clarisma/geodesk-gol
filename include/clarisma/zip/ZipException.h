// Copyright (c) 2024 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#ifdef CLARISMA_WITH_ZLIB

#pragma once
#include <stdexcept>
#include <zlib.h>

namespace clarisma {

class ZipException : public std::runtime_error
{
public:
    explicit ZipException(int zlibErrorCode) :
        std::runtime_error(zError(zlibErrorCode)),
        zlibErrorCode_(zlibErrorCode)
    {
    }

    explicit ZipException(const char* message) :
        std::runtime_error(message),
        zlibErrorCode_(0)
    {
    }

    /*
    template <typename... Args>
    explicit ZipException(const char* message, Args... args) :
        std::runtime_error(Format::format(message, args...)),
        zlibErrorCode_(0)
    {
    }
    */
    
    /*
    virtual const char* what() const noexcept override 
    {
        return message_.c_str();
    }
    */

    int zlibErrorCode() const noexcept { return zlibErrorCode_; }

private:
    // std::string message_;
    int zlibErrorCode_;
};

} // namespace clarisma

#endif