// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

// TODO: outdated:
// Precondition for *At() methods:
// - Handle opened with FILE_FLAG_OVERLAPPED for true positional I/O.
//   Passing OVERLAPPED to a non-overlapped file handle fails with
//   ERROR_INVALID_PARAMETER.

#pragma once
#include <cstdint>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
// Prevent Windows headers from clobbering min/max
#define NOMINMAX
#endif
#include <windows.h>

namespace clarisma {

enum class HttpError : uint32_t
{
    // TODO
};

} // namespace clarisma