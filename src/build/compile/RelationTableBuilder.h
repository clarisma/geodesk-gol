// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

class Membership;
class TileModel;
class TRelationTable;

class RelationTableBuilder
{
public:
    static TRelationTable* build(TileModel& tile, Membership* firstMembership);

private:
    static int countMemberships(Membership* pFirst);
    /*
    #ifdef GOL_BUILD_STATS
    int parentRelationCount = 0;
    int foreignParentRelationCount = 0;
    int wideTexParentRelationCount = 0;
    #endif
    */
};

