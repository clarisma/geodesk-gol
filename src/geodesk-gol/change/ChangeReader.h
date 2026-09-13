// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/cli/Console.h>
#include <clarisma/util/SimpleXmlParser.h>
#include <geodesk/geom/Coordinate.h>
#include "tag/TagTableModel.h"
#include "change/model/ChangeFlags.h"
#include "change/model/CFeature.h"

using namespace clarisma;
using namespace geodesk;

namespace geodesk
{
class StringTable;
}

class ChangeModel;
class CTagTable;

class ChangeReader : protected SimpleXmlParser
{
public:
    ChangeReader(ChangeModel& model, char* xml);
    void read();

private:
    void readChanges();
    void readFeature(ChangeFlags flags);
    int readFeatureAttributes();
    void readFeatureElements(int token);
    void readTag();
    void readNodeRef();
    void readMember();
    const CTagTable* setTags(ChangedFeatureBase* changed, bool checkIfArea);

    template <typename... Args>
    [[noreturn]] static void error(const char* message, Args... args)
    {
        std::string msg = Format::format(message, args...);
        Console::log(msg);
        throw std::runtime_error(msg);
    }

    static constexpr int ATTR_ID = 1;
    static constexpr int ATTR_VERSION = 2;
    static constexpr int ATTR_LON = 4;
    static constexpr int ATTR_LAT = 8;

    ChangeModel& model_;
    StringTable& strings_;
    int64_t id_;
    int attributes_;
    uint32_t version_;
    Coordinate xy_;
    TagTableModel tags_;
    std::vector<CFeatureStub*> members_;
    std::vector<CFeature::Role> roles_;
};
