// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "BuildSettings.h"
#include <geodesk/feature/GlobalStrings.h>
#include <geodesk/feature/ZoomLevels.h>
#include "IndexedKeysParser.h"
#include "ZoomLevelsParser.h"

BuildSettings::BuildSettings() :
    zoomLevels_(ZoomLevels::DEFAULT)
{
}

void BuildSettings::addIndexedKey(std::string_view key, int category)
{
    // TODO: split string at / to get multiple keys
    indexedKeys_.emplace_back(key, category);
}

void BuildSettings::setLevels(const char *s)
{
    zoomLevels_ = ZoomLevelsParser(s).parse();
}

void BuildSettings::setSource(const std::string_view path)
{
    sourcePath_ = path;
    // TODO: Check if file exists, adjust extension
}


void BuildSettings::setAreaRules(const char* rules)
{
    areaRules_ = AreaClassifier::Parser(rules).parseRules();
}

void BuildSettings::setIndexedKeys(const char *s)
{
    IndexedKeysParser parser(s);
    indexedKeys_ = parser.parse();
}


const char BuildSettings::DEFAULT_INDEXED_KEYS[] =
    "place "
    "highway "
    "railway "
    "aeroway "
    "aerialway "
    "tourism "
    "amenity "
    "shop "
    "craft "
    "power "
    "industrial "
    "man_made "
    "leisure "
    "landuse "
    "waterway "
    "natural/geological "
    "military "
    "historic "
    "healthcare "
    "office "
    "emergency "
    "building "
    "boundary "
    "building:part "
    "telecom "
    "communication "
    "route ";


void BuildSettings::complete()
{
    if(areaRules_.empty()) setAreaRules(AreaClassifier::DEFAULT);
    if(indexedKeys_.empty()) setIndexedKeys(DEFAULT_INDEXED_KEYS);
}

FeatureStore::IndexedKeyMap BuildSettings::keysToCategories() const
{
    FeatureStore::IndexedKeyMap keysToCategories;
    keysToCategories.reserve(indexedKeys_.size());
    for(int i=0; i<indexedKeys_.size(); i++)
    {
        keysToCategories[static_cast<uint16_t>(
            GlobalStrings::FIRST_INDEXED_KEY) + i] = indexedKeys_[i].category;
    }
    return keysToCategories;
}
