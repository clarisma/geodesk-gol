// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/util/Parser.h>
#include <clarisma/validate/Validate.h>
#include <geodesk/geom/Coordinate.h>

using namespace clarisma;
using namespace geodesk;

namespace geodesk {
class Filter;
}

class CoordinateParser : public Parser
{
public:
    explicit CoordinateParser(const char*s) :
        Parser(s)
    {
    }

protected:
    Coordinate parseCoordinate(bool latBeforeLon)
    {
        double lon = number();
        if(std::isnan(lon))
        {
            error(latBeforeLon ? "Expected latitude" : "Expected longitude");
        }
        accept(',');        // optional
        double lat = number();
        if(std::isnan(lat))
        {
            error(latBeforeLon ? "Expected longitude" : "Expected latitude");
        }
        if(latBeforeLon) std::swap(lon, lat);

        if(lon < -180.0 || lon > 180.0)
        {
            throw ValueException("Longitude (%f) must be between -180 and 180", lon);
        }
        if(lat < -90.0 || lat > 90.0)
        {
            throw ValueException("Latitude (%f) must be between -90 and 90", lat);
        }
        return Coordinate::ofLonLat(lon, lat);
    }
};
