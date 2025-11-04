// Copyright (c) 2024 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <memory>

namespace clarisma {

namespace UrlUtils
{
    inline bool isUrl(const char *url)
    {
        return strstr(url, "://");
    }
};

} // namespace clarisma
