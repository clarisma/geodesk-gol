// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <clarisma/cli/ConsoleWriter.h>

#ifdef GOL_DIAGNOSTICS
#define GOL_DEBUG if(clarisma::Console::verbosity() >= clarisma::Console::Verbosity::DEBUG) [[unlikely]] clarisma::ConsoleWriter(clarisma::Console::Stream::STDERR).timestampAndThread()
#else
#define GOL_DEBUG if(false) clarisma::ConsoleWriter()
#endif
