// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "CoordinateParser.h"
#include <vector>
#include <geodesk/geom/index/MCIndexBuilder.h>

using namespace clarisma;
using namespace geodesk;

namespace geodesk {
class Filter;
}

// TODO: Add option to return WithinFilter instead of IntersectsFilter
//  (needed for gol delete)

// TODO: Rename to AreaParser, create bbox if only two coords provided

class PolygonParser : public CoordinateParser
{
public:
    explicit PolygonParser(const char*s) :
        CoordinateParser(s),
        latBeforeLon_(false)
    {
    }

    std::unique_ptr<const Filter> parse();

private:
    void parseRings(char closingParen);
    void parseCoordinates(int maxCount, char closingParen);
    void addRing();
    void parseKeyword();

    enum GeoJsonType    // must be combinable via OR
    {
        GEOMETRY = 1,
        FEATURE = 2,
        FEATURE_COLLECTION = 4
    };

    void parseGeoJson();
    void parseFeatureOrGeometry(int allowedTypes, int recursionLevel);
    void skipJsonValue(int recursionLevel);
    std::string_view expectString();
    void typeError(int allowedTypes);

    std::vector<Coordinate> coords_;
    Box bounds_;
    MCIndexBuilder indexBuilder_;
    bool latBeforeLon_;
};
