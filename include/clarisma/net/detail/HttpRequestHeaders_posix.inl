// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

namespace clarisma {

inline void HttpRequestHeaders::add(const std::string_view& key, const std::string_view& value)
{
    headers_.emplace(key, value);
}

} // namespace clarisma
