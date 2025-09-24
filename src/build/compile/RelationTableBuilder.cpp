// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "RelationTableBuilder.h"
#include "tile/compiler/RelationTableWriter.h"
#include "tile/model/Membership.h"
#include "tile/model/TileModel.h"
#include "tile/model/TRelationTable.h"


int RelationTableBuilder::countMemberships(Membership* p)
{
    int count = 0;
    while(p)
    {
        count++;
        p = p->next_;
    }
    return count;
}

// TODO: is needsFixup being handled? yes
// maybe move flag-tracking to RTWriter
TRelationTable* RelationTableBuilder::build(TileModel& tile, Membership* firstMembership)
{
    assert(firstMembership);
    int count = countMemberships(firstMembership);
    assert(count);

    // We pre-allocate the space for the relation table using the
    // most conservative assumption: Every relation is foreign and
    // in a separate tile requiring a wide TIP-delta, hence 8 bytes
    // for each entry
    uint32_t maxSize = count * 8;
    TRelationTable* table = tile.beginRelationTable(maxSize);
    RelationTableWriter writer(table->handle(), table->data());
    Membership* membership = firstMembership;
    Tip prevTip;
        // we leave the TIP as 0 ("invalid") to cause the DIFFERENT_TILE
        // flag to be set for the first foreign relation
    Tex prevTex = Tex::RELATIONS_START_TEX;
    bool needsFixup = false;
    while(membership)
    {
        if(membership->isForeign())
        {
            ForeignFeatureRef ref = membership->foreignRelation();
            TexDelta texDelta = ref.tex - prevTex;
            if(ref.tip != prevTip)
            {
                if(prevTip.isNull()) prevTip = FeatureConstants::START_TIP;
                writer.writeForeignRelation(ref.tip - prevTip, texDelta);
                prevTip = ref.tip;
            }
            else
            {
                writer.writeForeignRelation(texDelta);
            }
            prevTex = ref.tex;
        }
        else
        {
            writer.writeLocalRelation(membership->localRelation());
            needsFixup = true;
        }
        membership = membership->next_;
    }
    writer.markLast();
    size_t trueSize = writer.ptr() - table->data();
    assert(trueSize <= maxSize);
    tile.arena().reduceLastAlloc(maxSize - trueSize);
    table->setSize(trueSize);
    /*
    #ifdef GOL_BUILD_STATS
    parentRelationCount = writer.memberCount;
    foreignParentRelationCount = writer.foreignMemberCount;
    wideTexParentRelationCount = writer.wideTexMemberCount;
    #endif
    */

    return tile.completeRelationTable(table, writer.hash(), needsFixup);
};

