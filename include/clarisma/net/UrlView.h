// Copyright (c) 2024 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#include <string_view>

namespace clarisma {

class UrlView 
{
public:
    UrlView(std::string_view url);

    std::string_view scheme() const noexcept { return scheme_; }
    std::string_view host() const noexcept { return host_; }
    int port() const noexcept;
    std::string_view path() const noexcept { return path_; }
    std::string_view query() const noexcept { return query_; }
    std::string_view fragment() const noexcept { return fragment_; }
    std::string_view origin() const noexcept;

private:
    std::string_view scheme_;
    std::string_view host_;
    std::string_view port_;
    std::string_view path_;
    std::string_view query_;
    std::string_view fragment_;
};

} // namespace clarisma