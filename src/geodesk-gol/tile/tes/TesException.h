// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <stdexcept>
#include <clarisma/text/Format.h>

class TesException : public std::runtime_error
{
public:
    explicit TesException(const char* message)
        : std::runtime_error(message) {}

    explicit TesException(const std::string& message)
        : std::runtime_error(message) {}

    template <typename... Args>
    explicit TesException(const char* message, Args... args)
        : std::runtime_error(clarisma::Format::format(message, args...)) {}
};

