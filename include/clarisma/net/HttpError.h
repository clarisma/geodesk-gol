// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#if defined(_WIN32)
#include "detail/HttpError_win.inl"
#else
#include "detail/HttpError_posix.inl"
#endif