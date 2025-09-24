// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "PolygonParser.h"
#include <clarisma/validate/Validate.h>
#include <geodesk/filter/IntersectsFilter.h>
#include <geodesk/geom/CoordinateSpanIterator.h>

std::unique_ptr<const Filter> PolygonParser::parse()
{
    skipWhitespace();
    if (*pNext_ == '{')
    {
        pNext_++;
        parseGeoJson();
    }
    else
    {
        parseKeyword();
        parseRings(0);
    }
    if(!coords_.empty()) addRing();
    return std::make_unique<const IntersectsPolygonFilter>(bounds_, indexBuilder_.build(bounds_));
}


void PolygonParser::parseKeyword()
{
    char buf[32];

    int n = 0;
    while(n < 30)
    {
        char ch = *pNext_;
        if(ch >= 'A' && ch <= 'Z')
        {
            buf[n] = ch - 'A' + 'a';
        }
        else if(ch >= 'a' && ch <= 'z')
        {
            buf[n] = ch;
        }
        else
        {
            break;
        }
        n++;
        pNext_++;
    }
    if(n > 0)
    {
        std::string_view keyword(buf, n);
        if(keyword != "polygon" && keyword != "multipolygon" && keyword != "lonlat")
        {
            if(keyword == "latlon")
            {
                latBeforeLon_ = true;
            }
            else
            {
                error("Expected 'polygon', 'multipolygon', 'lonlat' or 'latlon', or <coordinates>");
            }
        }
    }
}


void PolygonParser::addRing()
{
    if(coords_.size() < 3)
    {
        error("Expected at least 3 coordinate pairs");
    }
    if(coords_.back() != coords_.front())
    {
        coords_.push_back(coords_.front());
    }
    indexBuilder_.segmentize<const std::vector<Coordinate>&,CoordinateSpanIterator>(coords_);
    coords_.clear();
}

void PolygonParser::parseRings(char closingParen)
{
    skipWhitespace();
    char childOpenParen = *pNext_;
    if(childOpenParen == '(' || childOpenParen == '[')
    {
        pNext_++;
        char childClosingParen = childOpenParen == '(' ? ')' : ']';
        parseRings(childClosingParen);
        if(!coords_.empty())
        {
            assert(coords_.size() == 1);
            for(;;)
            {
                accept(',');
                expect(childOpenParen);
                parseCoordinates(1,childClosingParen);
                if(accept(closingParen)) break;
            }
            addRing();
            return;
        }
        while(!accept(closingParen))
        {
            accept(',');
            expect(childOpenParen);
            parseRings(childClosingParen);
        }
        return;
    }
    parseCoordinates(INT_MAX, closingParen);
    if(coords_.size() > 1) addRing();
}


void PolygonParser::parseCoordinates(int maxCount, char closingParen)
{
    int count = 0;
    while(count < maxCount)
    {
        Coordinate c = parseCoordinate(latBeforeLon_);
        // TODO: clamping (in parseCoordinate)
        coords_.push_back(c);
        bounds_.expandToInclude(c);

        count++;
        if(closingParen)
        {
            if(accept(closingParen)) return;
        }
        else
        {
            if(*pNext_ == 0) break;
        }
        accept(',');        // optional
    }
    if(closingParen) expect(closingParen);
}

void PolygonParser::parseGeoJson()
{
    parseFeatureOrGeometry(GeoJsonType::GEOMETRY | GeoJsonType::FEATURE |
        GeoJsonType::FEATURE_COLLECTION, 0);
}

void PolygonParser::parseFeatureOrGeometry(int allowedTypes, int recursionLevel)  // NOLINT recursive
{
    int type = 0;
    int keys = 0;

    enum Key
    {
        COORDINATES = 1,
        GEOMETRY = 2,
        FEATURES = 4
    };

    // TODO: enforce max nesting!

    for (;;)
    {
        std::string_view key = expectString();
        expect(':');
        if (key == "type")
        {
            std::string_view value = expectString();
            if (value == "Polygon" || value == "MultiPolygon")
            {
                if ((allowedTypes & GeoJsonType::GEOMETRY) == 0)
                {
                    typeError(allowedTypes);
                }
                type = GeoJsonType::GEOMETRY;
            }
            else if (value == "Feature")
            {
                if ((allowedTypes & GeoJsonType::FEATURE) == 0)
                {
                    typeError(allowedTypes);
                }
                type = GeoJsonType::FEATURE;
            }
            else if (value == "FeatureCollection")
            {
                if ((allowedTypes & GeoJsonType::FEATURE_COLLECTION) == 0)
                {
                    typeError(allowedTypes);
                }
                type = GeoJsonType::FEATURE_COLLECTION;
            }
            else
            {
                typeError(allowedTypes);
            }
        }
        else if (key == "coordinates")
        {
            keys |= Key::COORDINATES;
            expect('[');
            parseRings(']');
        }
        else if (key == "geometry")
        {
            keys |= Key::GEOMETRY;
            expect('{');
            parseFeatureOrGeometry(GeoJsonType::GEOMETRY, recursionLevel + 1);
        }
        else if (key == "features")
        {
            keys |= Key::FEATURES;
            expect('[');
            expect('{');
            parseFeatureOrGeometry(GeoJsonType::FEATURE |
                GeoJsonType::FEATURE_COLLECTION, recursionLevel + 1);
            if (*pNext_ == ',')
            {
                error("Only one Feature allowed");
            }
            expect(']');
        }
        else
        {
            skipJsonValue(recursionLevel + 1);
        }
        if (accept('}')) break;
        expect(',');
    }
    switch (type)
    {
    case 0:
        error("Missing 'type'");
        return;
    case GeoJsonType::GEOMETRY:
        if (keys != Key::COORDINATES)
        {
            error("Must have 'coordinates'");
        }
        return;
    case GeoJsonType::FEATURE:
        if (keys != Key::GEOMETRY)
        {
            error("Must have 'geometry'");
        }
        return;
    case GeoJsonType::FEATURE_COLLECTION:
        if (keys != Key::FEATURES)
        {
            error("Must have 'features'");
        }
        return;
    }
}

void PolygonParser::typeError(int allowedTypes)
{
    error("Expected type %s",
        allowedTypes == GeoJsonType::GEOMETRY ?
            "'Polygon' or 'MultiPolygon'" :
                (allowedTypes == GeoJsonType::FEATURE ?
                "'Feature' or 'FeatureCollection'" :
                "'Polygon', 'MultiPolygon', 'Feature' or 'FeatureCollection'"));
}


std::string_view PolygonParser::expectString()
{
    ParsedString s = string();
    if (s.isNull())
    {
        error("Expected string");
        return std::string_view();
    }
    return s.asStringView();
}

void PolygonParser::skipJsonValue(int recursionLevel)  // NOLINT recursive
{
    ParsedString s = string();
    if (!s.isNull())
    {
        skipWhitespace();
        return;
    }
    double d = number();
    if (!std::isnan(d)) return;

    // TODO: true, false, null

    if (recursionLevel >= 128)
    {
        error("Excessive nesting");
        return;
    }
    if (*pNext_ == '[')
    {
        pNext_++;
        skipWhitespace();
        for (;;)
        {
            skipJsonValue(recursionLevel+1);
            if (accept(']')) return;
            expect(',');
        }
    }
    if (*pNext_ == '{')
    {
        pNext_++;
        skipWhitespace();
        for (;;)
        {
            expectString();
            skipWhitespace();
            expect(':');
            skipJsonValue(recursionLevel+1);
            if (accept('}')) return;
            expect(',');
        }
    }
}
