// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <geodesk/feature/RelationPtr.h>
#include <geodesk/format/GolStringSet.h>


namespace geodesk {
class StringTable;
}

using namespace geodesk;

class CompleteRelationSet
{
public:
    void update(std::string_view types, const StringTable& strings);
    bool contains(RelationPtr rel) const noexcept;

private:
    enum { NONE, ALL, SOME };

    GolStringSet acceptedTypes_;
    int completeRelations_ = NONE;
    int typeGlobalKey_ = 0;
};

