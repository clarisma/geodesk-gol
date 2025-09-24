// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangeAction.h"
#include "ChangeModel.h"

void ChangeAction::apply(ChangeModel& model)
{
    // TODO: This is inefficient, in cases like Membership::Added,
    //  we could pass the pointer to the stub in order to avoid
    //  looking up the feature by typedId
    ChangedFeatureBase* changed = model.getChanged(typedId());
    if (ref_ != CRef::UNKNOWN)  [[likely]]
    {
        if(isRefSE_)    [[unlikely]]
        {
            changed->offerRefSE(ref_);
        }
        else
        {
            changed->offerRef(ref_);
        }
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
    if (changed->typedId() == TypedFeatureId::ofNode(2422945180))
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
    ChangedFeature2D* way = ChangedFeature2D::cast(changed);
    assert(!ref_.tip().isNull());
    DataPtr pTile = model.store()->fetchTile(ref_.tip());
    assert(pTile);
    WayPtr pastWay(ref_.getFeature(pTile));
    assert(!pastWay.isNull());
    way->setBounds(pastWay.bounds());
    if (way->memberCount() == 0)
    {
        way->setMembers(model.loadWayNodes(ref_.tip(), pTile, pastWay));
    }
    way->addFlags(ChangeFlags::GEOMETRY_CHANGED);
}

void NodeBecomesCoincident::apply(ChangedFeatureBase* changed)
{
    changed->addFlags(ChangeFlags::NODE_WILL_SHARE_LOCATION);
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