// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangedFeature2D.h"

#include <clarisma/util/log.h>
#include <geodesk/feature/FeatureNodeIterator.h>

#include "ChangedNode.h"

void ChangedFeature2D::compareWayMembers(FeatureStore* store, WayPtr pastWay)
{
    assert(!pastWay.isNull());
    FeatureNodeIterator iter(store, pastWay);
    for(const CFeatureStub* nodeStub : members())
    {
        const CFeature* node = nodeStub->get();
        if(!node->ref().tip().isNull())
        {
            if(node->isChanged() && test(ChangedNode::cast(node)->flags(),
                ChangeFlags::TILES_CHANGED))
            {
                // If a feature node of a way has moved tiles, we always
                // have to write the node table
                return;
            }
            NodePtr pastNode = iter.next();
            if(pastNode.isNull()) return;
            if(pastNode.id() != node->id()) return;
        }
    }
    if(!iter.next().isNull()) return;
    clearFlags(ChangeFlags::MEMBERS_CHANGED);
}
