// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangeAction.h"
#include "ChangeModel.h"
#include "geodesk/feature/FeatureBase.h"

// TODO: Let TCA do all the principal work; e.g. retrieve nodes of
//  implicitly changed ways while the tile is "hot" and stash
//  them in the TCA's arena, then copy them into the model
//  Fetching the nodes for an implicitly changed way while applying
//  the actions often requires fetching a tile that is no longer in
//  cache, hence we're seeing 30s of runtime to process actions for
//  a week-long update

void ChangeAction::apply(ChangeModel& model)
{
    // TODO: This is inefficient, in cases like Membership::Added,
    //  we could pass the pointer to the stub in order to avoid
    //  looking up the feature by typedId
    ChangedFeatureBase* changed = model.getChanged(typedId());

    if (typedId() == TypedFeatureId::ofWay(682409966))
    {
        LOGS << "!!!";
    }

    if (ref_ != CRef::UNKNOWN)  [[likely]]
    {
        if(isRefSE_)    [[unlikely]]
        {
            if (typedId().isNode())
            {
                LOGS << "Illegal SE ref for " << typedId() <<
                    " in change action " << type_;
            }
            changed->offerRefSE(ref_);
        }
        else
        {
            changed->offerRef(ref_);
        }
    }

    if (!changed->isNode())
    {
        ChangedFeature2D* changed2d = ChangedFeature2D::cast(changed);
        FeaturePtr feature = changed2d->getFeature(model.store());
        if (!feature.isNull()) [[likely]]
        {
            changed2d->initFrom(feature);
        }

        // TODO: WE need to consolidate this, there are lots
        //  of places where we may be creating an implicit
        //  feature change, and hence need to get the original
        //  area flag and bounds
        //  Ideally, the TCA should look up the change already
        //  Maybe make actions more granular
        //  Move as much of the decision-making into the TCA
        //  because it executes in parallel
    }

    switch (action_)
    {
    case RELATION_MEMBER_ADDED:
        reinterpret_cast<MembershipChange::Added*>(this)->apply(changed);
        break;

    case RELATION_MEMBER_REMOVED:
        reinterpret_cast<MembershipChange::Removed*>(this)->apply(changed);
        break;

    case NODE_BECOMES_COINCIDENT:
        reinterpret_cast<NodeBecomesCoincident*>(this)->apply(changed);
        break;

    case NODE_REMOVED_FROM_WAY:
        reinterpret_cast<NodeRemovedFromWay*>(this)->apply(changed);
        break;

    case NODE_BECOMES_WAYNODE:
        reinterpret_cast<NodeBecomesWaynode*>(this)->apply(changed);
        break;

    case IMPLICIT_WAY_GEOMETRY_CHANGE:
        reinterpret_cast<ImplicitWayGeometryChange*>(this)->apply(model, changed);
        break;

    default:
        assert(false);
        break;
    }
}


void MembershipChange::Added::apply(ChangedFeatureBase* changed)
{
    if (changed->typedId() == TypedFeatureId::ofRelation(5160381))
    {
        LOGS << "ChangeAction: " << changed->typedId() << " added to "
            << parentRelation_->typedId();
    }
    changed->addMembershipChange(this);
    changed->addFlags(ChangeFlags::ADDED_TO_RELATION | ChangeFlags::RELTABLE_CHANGED);
}

void MembershipChange::Removed::apply(ChangedFeatureBase* changed)
{
    changed->addMembershipChange(this);
    changed->addFlags(ChangeFlags::REMOVED_FROM_RELATION | ChangeFlags::RELTABLE_CHANGED);
}

void ImplicitWayGeometryChange::apply(ChangeModel& model, ChangedFeatureBase* changed) const
{
    if(1154460013 == changed->id())
    {
        LOGS << "Action: Implicit geometry change for way/" << changed->id();
    }
    ChangedFeature2D* way = ChangedFeature2D::cast(changed);
    assert(!ref_.tip().isNull());
    TilePtr pTile = model.store()->fetchTile(ref_.tip());
    assert(pTile);
    WayPtr pastWay(ref_.getFeature(pTile));
    assert(!pastWay.isNull());
    way->initFrom(pastWay);
        // We need to always initialize the bounds of a ChangedFeature2D
        // with its old bounds
    if (way->memberCount() == 0)
    {
        way->setMembers(model.loadWayNodes(ref_.tip(), pTile, pastWay));
    }
    if (!way->isChangedExplicitly())
    {
        // If a node causes a geometry change to a way that has
        // not been explicitly changed, we know it will belong
        // to that way, hence it will be a future way node
        // and cannot become an orphan
        node_->markAsFutureWaynode();
    }
    way->addFlags(ChangeFlags::GEOMETRY_CHANGED);
}

void NodeBecomesCoincident::apply(ChangedFeatureBase* changed)
{
    changed->addFlags(ChangeFlags::FLAGGED_SHARED_LOCATION | ChangeFlags::FLAGS_CHANGED);
    if (changed->xy().isNull()) changed->setXY(xy_);
}

void NodeRemovedFromWay::apply(ChangedFeatureBase* changed)
{
    changed->addFlags(ChangeFlags::REMOVED_FROM_WAY);
    if (changed->xy().isNull()) changed->setXY(xy_);
}

void NodeBecomesWaynode::apply(ChangedFeatureBase* changed)
{
    // Do nothing; it is sufficient that we ensure that the
    // node is marked as changed, since changed node processing
    // will deal with change in waynode status
}