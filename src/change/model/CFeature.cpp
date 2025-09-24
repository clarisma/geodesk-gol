// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <geodesk/feature/ParentRelationIterator.h>
#include "ChangedFeatureBase.h"
#include "CRelationTable.h"

// TODO: can only call if not moved from all tiles
// TODO: narrow scope: rename to mayMemberKeepTex()?
// TODO: needs to move to ChangeModel
/*
bool CFeature::willBeForeignMember(FeatureStore* store) const
{
    if(isChanged())
    {
        auto changed = ChangedFeatureBase::cast(this);
        if (changed->is(ChangeFlags::RELTABLE_LOADED))
        {
            const CRelationTable* rels = changed->parentRelations();
            if (!rels) return false;
            for (const CFeatureStub* relStub : rels->relations())
            {
                if (isForeignMemberOf(relStub->get()))
                {
                    return true;
                }
            }
            return false;
        }
    }

    FeaturePtr past = getFeature(store);
    assert(!past.isNull());
    if (!past.isRelationMember()) return false;
    ParentRelationIterator iter(store, past.relationTableFast());
    for (;;)
    {
        RelationPtr pastParent = iter.next();
        if (pastParent.isNull()) break;
        const CFeature* rel = peekFeature(TypedFeatureId::ofRelation(pastParent.id()));
        if (rel)
        {
            if (isForeignMemberOf(rel)) return true;
        }

        // TODO: obtain the parent's TIPs based on its bbox
    }
}
*/