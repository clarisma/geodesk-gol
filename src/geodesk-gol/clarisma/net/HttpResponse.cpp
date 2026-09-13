// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#if defined(_WIN32)
#include "HttpResponse_windows.cxx"
#elif defined(__linux__) || defined(__APPLE__)
// #include "HttpResponse_linux.cxx"
#else
#error "Platform not supported"
#endif

