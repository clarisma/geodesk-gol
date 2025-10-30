// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <clarisma/net/UrlView.h>
#include <charconv>

namespace clarisma {

UrlView::UrlView(std::string_view url)
{
    // Look for "://" to see if a scheme is provided.
    auto schemePos = url.find("://");
    std::string_view remainder = url;
    if (schemePos != std::string_view::npos)
    {
        scheme_ = url.substr(0, schemePos);
        remainder.remove_prefix(schemePos + 3);
    }
    else
    {
        scheme_ = "http";
    }

    // Extract host and optional port from the remainder.
    size_t hostEnd = remainder.find_first_of("/?#");
    std::string_view hostPort;
    if (hostEnd != std::string_view::npos)
    {
        hostPort = remainder.substr(0, hostEnd);
        remainder.remove_prefix(hostEnd);
    }
    else
    {
        hostPort = remainder;
        remainder = "";
    }

    // Split host and port if a colon is present.
    size_t colonPos = hostPort.find(':');
    if (colonPos != std::string_view::npos)
    {
        host_ = hostPort.substr(0, colonPos);
        port_ = hostPort.substr(colonPos + 1);
    }
    else
    {
        host_ = hostPort;
    }

    // If the remainder begins with a '/', that's the path.
    if (!remainder.empty() && remainder[0] == '/')
    {
        size_t pathEnd = remainder.find_first_of("?#");
        if (pathEnd != std::string_view::npos)
        {
            path_ = remainder.substr(0, pathEnd);
            remainder.remove_prefix(pathEnd);
        }
        else
        {
            path_ = remainder;
            remainder = "";
        }
    }

    // If a query is present, it starts with '?'.
    if (!remainder.empty() && remainder[0] == '?')
    {
        remainder.remove_prefix(1); // Remove '?'
        size_t queryEnd = remainder.find('#');
        if (queryEnd != std::string_view::npos)
        {
            query_ = remainder.substr(0, queryEnd);
            remainder.remove_prefix(queryEnd);
        }
        else
        {
            query_ = remainder;
            remainder = "";
        }
    }

    // If a fragment is present, it starts with '#'.
    if (!remainder.empty() && remainder[0] == '#')
    {
        remainder.remove_prefix(1); // Remove '#'
        fragment_ = remainder;
    }
}

int UrlView::port() const noexcept
{
    if (!port_.empty())
    {
        int port = 0;
        auto result = std::from_chars(
            port_.data(), port_.data() + port_.size(), port);
        if (result.ec == std::errc()) return port;
    }

    if (scheme_ == "https") return 443;
    if (scheme_ == "ftp") return 21;
    return 80;
}

std::string_view UrlView::origin() const noexcept
{
    if (scheme_.empty() || host_.empty()) return {};
    const char* start = scheme_.data();
    const char* end = port_.empty() ? host_.end() : port_.end();
    return { start, end };
}

} // namespace clarisma
