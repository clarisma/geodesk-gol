// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

class Installer
{
public:
    static void upgrade();

    static constexpr const char* PLATFORM =
#ifdef _WIN32
        "win";
#elif __APPLE__
         "macos";
#else
         "linux";
#endif
};
