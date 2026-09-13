// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ZoomLevelsParser.h"
#include <cmath>


ZoomLevels ZoomLevelsParser::parse()
{
    ZoomLevels levels(1);
    for(;;)
    {
        skipWhitespace();
        double d = number();
        int level = static_cast<int>(d);
        if(std::isnan(d) || d < 0 || d > 12 || level != d)
        {
            error("Expected number (0 to 12 inclusive)");
        }
        levels.add(level);
        if(!accept(',') && !accept('/')) break;
    }
    levels.check();
    return levels;
}