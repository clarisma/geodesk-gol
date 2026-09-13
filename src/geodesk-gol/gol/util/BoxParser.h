// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "CoordinateParser.h"
#include <geodesk/geom/Box.h>

// Consolidate into PolygonParser

class BoxParser : public CoordinateParser
{
public:
    using CoordinateParser::CoordinateParser;
    Box parse()
    {
        Coordinate bottomLeft = parseCoordinate(false);
        if(accept(',')) skipWhitespace();       // optional ,
        Coordinate topRight = bottomLeft;
        if(*pNext_ != 0) topRight = parseCoordinate(false);
        return geodesk::Box(bottomLeft.x, bottomLeft.y, topRight.x, topRight.y);
    }
};
