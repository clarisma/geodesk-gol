// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <clarisma/util/log.h>

namespace clarisma {

inline void HttpRequestHeaders::add(const std::string_view& key, const std::string_view& value)
{
    // ": " (2) + "\n" (1)
    headers_.reserve(headers_.size() + key.size() + 2 + value.size() + 1);
    headers_.append(key);
    headers_.append(": ");
    headers_.append(value);
    headers_.push_back('\n');
    LOGS << "Added header: " << key << ": " << value;
}

} // namespace clarisma
