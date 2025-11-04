// Copyright (c) 2024 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#include <cstdint>
#include <string_view>

namespace clarisma {

class SimpleUrlView
{
public:
    SimpleUrlView(std::string_view url)
    {
        start_ = url.data();    // NOLINT escape
        originSize_ = 0;
        pathSize_ = 0;
        auto schemePos = url.find("://");
        if (schemePos == std::string_view::npos)
        {
            return;
        }
        auto hostStart = schemePos + 3;
        if (hostStart >= url.size()) return;
        auto pathPos = url.find('/', hostStart);
        if (pathPos == std::string_view::npos)
        {
            // no path: everything is origin
            originSize_ = static_cast<uint32_t>(url.size());
            return;
        }
        originSize_ = static_cast<uint32_t>(pathPos);
        pathSize_ = static_cast<uint32_t>(url.size() - pathPos);
    }

    std::string_view origin() const { return { start_, originSize_ }; }
    std::string_view path() const
    {
        return { start_ + originSize_, pathSize_ };
    }

private:
    const char* start_;
    uint32_t originSize_;
    uint32_t pathSize_;
};

} // namespace clarisma