// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/util/Parser.h>
#include <geodesk/feature/ZoomLevels.h>

using namespace geodesk;

class ZoomLevelsParser : public clarisma::Parser
{
public:
    explicit ZoomLevelsParser(const char *s) : Parser(s) {}

    ZoomLevels parse();
};
