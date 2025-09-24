// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileChangeAnalyzer.h"
#include <clarisma/util/log.h>
#include <geodesk/feature/MemberIterator.h>
#include <geodesk/feature/WayNodeIdIterator.h>

#include "change/model/ChangeAction.h"
#include "change/model/ChangeModel.h"
#include "change/model/ChangedNode.h"

/// - If relation is in the ChangeModel:
///   - Store its ref
///   - If it is explicitly changed:
///     - Check if tags changed (clear flag if not)
///
void TileChangeAnalyzer::readNode(NodePtr node)
{
    if(24940645 == node.id())
    {
        LOGS << "Analyzing node/" << node.id();
    }
    Coordinate xy = node.xy();
    CFeature* f = model_.peekFeature(TypedFeatureId::ofNode(node.id()));
    if (f)
    {
        CRef ref = refOfLocal(node);
        f->setRef(ref);
        if(node.ptr() != f->getFeature(model_.store()).ptr())
        {
            LOGS << node.typedId() << " != " << f->getFeature(model_.store()).typedId();
        }
        assert(node.ptr() == f->getFeature(model_.store()).ptr());
        if(f->isChanged())
        {
            ChangedNode* changed = ChangedNode::cast(f);
            if(f->xy() == node.xy())
            {
                changed->clearFlags(ChangeFlags::GEOMETRY_CHANGED);
            }
            compareTags(changed, node);
            return;
                // Skip the check for coincident nodes below;
                // if the node has changed, this check has already
                // been performed as part of ChangeModel::prepareNodes()
                // (If we continue, we would need to check if the
                // node at the same location is the changed node itself)
        }
        if (f->isFutureWaynode() && (node.flags() & FeatureFlags::WAYNODE) == 0)
        {
            // This node will belong to a way, but its past version
            // does not have the waynode_flag set. (We don't need this
            // check for an explicitly changed node, because
            // Updater::processNode() checks all changed nodes for
            // a change in waynode status)
            addAction<NodeBecomesWaynode>(node.id(), ref);
        }
        f->setXY(xy);
    }

    // Check if at same location as a future node location
    ChangedNode* coincidentNode = model_.nodeAtFutureLocation(xy);
    if(coincidentNode)
    {
        addAction<NodeBecomesCoincident>(node.id(), node.xy(), refOfLocal(node));
        addAction<NodeBecomesCoincident>(coincidentNode->id(), node.xy(), CRef::UNKNOWN);
    }
}

/// If way is in the ChangeModel:
/// - Store its ref (NW or SE)
/// - If it is explicitly changed (and we're processing the NW twin):
///   - Check if its tags (and clear flag if not)
///   - Check if
///
///
///

void TileChangeAnalyzer::readWay(WayPtr way)
{
    if(231875725 == way.id())
    {
        LOGS << "Analyzing way/" << way.id();
    }

    CFeature* f = model_.peekFeature(TypedFeatureId::ofWay(way.id()));
    if (f)
    {
        CRef ref = refOfLocal(way);
        if(!way.hasNorthwestTwin()) [[likely]]
        {
            f->setRef(ref);
            Box bounds = way.bounds();
            if (bounds.maxX() <= tileMaxX_ && bounds.minY() >= tileMinY_)
            {
                f->setRefSE(CRef::SINGLE_TILE);
            }
            if(f->isChanged())
            {
                ChangedFeature2D* changed = ChangedFeature2D::cast(f);

                if(231875725 == way.id())
                {
                    LOGS << "way/" << way.id() << "is changed (version " << changed->version() << ")";
                }

                changed->setBounds(bounds);
                compareWayNodes(changed, way);
                    // We call compareWayNodes() also for deleted ways,
                    // since we need to extract their coordinates and
                    // refs in case another way or relation uses them,
                    // and also to mark any non-deleted nodes as
                    // REMOVED_FROM_WAY
                if(!changed->isDeleted())
                {
                    compareTags(changed, way);
                    compareAreaStatus(changed, way);
                }
                return;
            }
        }
        else
        {
            f->setRefSE(ref);
        }
    }

    scanWayNodes(way);

    // For non-chnaged ways, we need to scan both NW and SE twins
    // -- Really? Why??
    // TODO: We could omit testing SE twins if we don't use index-
    //  assisted search, since in that case we're guaranteed to
    //  find every way
}

/// If relation is in the ChangeModel:
/// - Get the ref to the existing relation
/// - If it is explicitly changed (and we're processing the NW twin):
///   - Check if its member table changed (clear MEMBERS_CHANGED if not)
///   - Determine which features have been added/removed
///
void TileChangeAnalyzer::readRelation(RelationPtr relation)
{
    CFeature* f = model_.peekFeature(TypedFeatureId::ofRelation(relation.id()));
    if (f)
    {
        CRef ref = refOfLocal(relation);
        if(!relation.hasNorthwestTwin()) [[likely]]
        {
            f->setRef(ref);
            Box bounds = relation.bounds();
            if (bounds.maxX() <= tileMaxX_ && bounds.minY() >= tileMinY_)
            {
                f->setRefSE(CRef::SINGLE_TILE);
            }
            if(f->isChanged())
            {
                ChangedFeature2D* changed = ChangedFeature2D::cast(f);
                changed->setBounds(bounds);
                if(!changed->isDeleted())
                {
                    compareTags(changed, relation);
                }
                checkMembers(changed, relation);
            }
        }
        else
        {
            f->setRefSE(ref);

            // TODO: Should we read members for the SE twin as well,
            //  so we pick up additional refs of dual-tile members?
            //  If so, careful about setting flags, can only set
            //  them if processing the NW twin, otherwise this will
            //  create a data race (writes to ChangeFeature2D are not
            //  synchronized)
        }
    }

    // TODO
}

// TODO: Can we avoid the hashtable for exports?
//  We could read the exports last, and replace the refs of relevant
//  features with a tex -- no, need TEX upfront to generate ChangeActions?
CRef TileChangeAnalyzer::refOfLocal(FeaturePtr feature) const
{
    int32_t handle = handleOfLocal(feature);
    auto it = exports_.find(handle);
    if(it != exports_.end())
    {
        return CRef::ofExported(tip_, it->second);
    }
    return CRef::ofNotExported(tip_, handle);
}


void TileChangeAnalyzer::compareTags(ChangedFeatureBase* f, FeaturePtr p)
{
    TagTablePtr pTags = p.tags();
    const CTagTable* tags = f->tagTable();
    if(!tags)
    {
        if(!pTags.isEmpty()) return;
    }
    else
    {
        if(!tags->equals(model_, pTags.ptr().ptr() - pTile_.ptr(), pTags)) return;
    }
    f->clearFlags(ChangeFlags::TAGS_CHANGED);
    // TODO: concurrent access?

    unchangedTags_++;
}

static CFeature DUMMY_NODE;
static CFeatureStub* DUMMY_NODES[1] = { &DUMMY_NODE };

void TileChangeAnalyzer::compareWayNodes(ChangedFeature2D* changed, WayPtr way)
{
    if(440389771 == way.id())
    {
        LOGS << "Comparing way nodes of way/" << way.id();
    }

    WayNodeIterator iter(model_.store(), way, false, true);
    bool wayGeometryChanged = false;
    bool waynodeIdsChanged = iter.storedRemaining() != changed->memberCount();
    CFeatureStub** pFutureNode = changed->members().data();
    pFutureNode = pFutureNode ? pFutureNode : DUMMY_NODES;
        // guard against null pointer for deleted ways
        // (.osc file does not always contain the nodes of a deleted way)

    for(;;)
    {
        WayNodeIterator::WayNode node = iter.next();
        if(node.id == 0) break;
        auto[nodeGeometryChanged, modelNode] = checkWayNode(node);
        wayGeometryChanged |= nodeGeometryChanged;
        if(modelNode==nullptr || !modelNode->isFutureWaynode())
        {
            // If the node is not in the model, or it has not been marked
            // as belonging to a way by ChangeModel::prepareWays(), this
            // means it's been dropped from a way (In a later step, we'll
            // check if the node loses its waynode status because it no
            // longer belongs to any way, and possibly turn it into an
            // orphan)
            if(modelNode==nullptr || !modelNode->isChanged() ||
                !ChangedNode::cast(modelNode)->isDeleted())
            {
                // We don't perform this action for deleted nodes (most common
                // case whenever a node is dropped), as it doesn't matter then
                addAction<NodeRemovedFromWay>(node.id, node.xy, refOfWayNode(node));
                // TODO: check if this action is safe to repeat (same node may be dropped
                //  multiple times form same way)
            }
        }
        CFeature* futureNode = (*pFutureNode)->get();
        bool differentNodeId = futureNode->id() != node.id;
        waynodeIdsChanged |= differentNodeId;
        assert(!waynodeIdsChanged == 0 || !waynodeIdsChanged == 1);
        pFutureNode += !waynodeIdsChanged;
            // since we check upfront whether node count of past and
            // future way are different, and only advance pointer
            // if ways are not different, we avoid reading past the
            // end of the future way-nodes if past has same nodes,
            // but more than future
    }

    // If waynode IDs are different, geometry is always considered changed
    // (even in theoretical case that the geometry remains the same)
    wayGeometryChanged |= waynodeIdsChanged;

    /*
    if(!wayGeometryChanged)
    {
        LOGS << changed->typedId() << ": geometry unchanged in version " << changed->version();
    }
    */

    ChangeFlags flagsToClear = wayGeometryChanged ? ChangeFlags::NONE : ChangeFlags::GEOMETRY_CHANGED;
    flagsToClear |= waynodeIdsChanged ? ChangeFlags::NONE : ChangeFlags::WAYNODE_IDS_CHANGED;
    changed->clearFlags(flagsToClear);
}


void TileChangeAnalyzer::compareAreaStatus(ChangedFeature2D* changed, FeaturePtr feature)
{
    bool wasArea = feature.isArea();
    bool willBeArea = (changed->flags() & ChangeFlags::WILL_BE_AREA) != ChangeFlags::NONE;
    if(wasArea != willBeArea) changed->addFlags(ChangeFlags::AREA_STATUS_CHANGED);
}

/// This method:
///
/// What it doesn't do (and why):
/// - It doesn't clear GEOMETRY_CHANGED if a changed node's coordinates
///   remain the same (because readNode already does this for feature nodes,
///   and for anonymous nodes it does not matter as they only exist as
///   vertexes of ways); if an anon node becomes a feature, its xy must
///   be written into the TES even if it did not actually change
///
void TileChangeAnalyzer::scanWayNodes(WayPtr way)
{
    /*
    if(112394938 == way.id())
    {
        Console::log("Scanning nodes of way/%lld in tip %d", way.id(), tip_);
    }
    */
    bool wayGeometryChanged = false;
    WayNodeIterator iter(model_.store(), way, false, true);
    for(;;)
    {
        WayNodeIterator::WayNode node = iter.next();
        if(node.id == 0) break;
        auto [nodeGeometryChanged, modelNode] = checkWayNode(node);
        wayGeometryChanged |= nodeGeometryChanged;
    }

    if(wayGeometryChanged)
    {
        // The way's geometry changed, since one or more nodes have
        // changed coordinates. If the way has not been explicitly
        // changed, we need to report is as *implicitly changed*

        // LOGS << "way/" << way.id() << " changed implicitly";

        addAction<ImplicitWayGeometryChange>(way.id(),
            refOfLocal(way), way.hasNorthwestTwin());
    }
}


CRef TileChangeAnalyzer::refOfWayNode(const WayNodeIterator::WayNode& node) const
{
    if(node.feature.isNull()) return CRef::ANONYMOUS_NODE;
    if(node.foreign.isNull()) return refOfLocal(node.feature);
    return CRef::ofForeign(node.foreign);
}


TileChangeAnalyzer::WayNodeCheckResult TileChangeAnalyzer::checkWayNode(
    const WayNodeIterator::WayNode& node)
{
    if(node.id == 1086081182)
    {
        LOGS << "Checking node/" << node.id;
    }

    bool geometryChanged = false;
    CFeature* f = model_.peekFeature(TypedFeatureId::ofNode(node.id));
    if(f)
    {
        f->setRef(refOfWayNode(node));
        if(f->isChanged())
        {
            if(node.xy != f->xy())
            {
                geometryChanged = true;
            }
            /*      // we don't care about geometry change of anon node
            else
            {
                ChangedNode::cast(f)->clearFlags(ChangeFlags::GEOMETRY_CHANGED);
            }
            */
            return {geometryChanged,f};
                // Skip the check for coincident nodes below;
                // if the node has changed, this check has already
                // been performed as part of ChangeModel::prepareNodes()
                // (If we continue, we would need to check if the
                // node at the same location is the changed node itself)
        }
        assert(!node.xy.isNull());
        f->setXY(node.xy);
    }

    ChangedNode* coincidentNode = model_.nodeAtFutureLocation(node.xy);
    if(coincidentNode && node.feature.isNull())
    {
        // Report this anonymous node as being coincident with
        // a changed node (No need to do this for feature nodes,
        // since readNode() does this already)
        // Also report the other node

        addAction<NodeBecomesCoincident>(node.id, node.xy, CRef::ANONYMOUS_NODE);
        addAction<NodeBecomesCoincident>(coincidentNode->id(), node.xy, CRef::UNKNOWN);
    }
    return {geometryChanged,f};
}

void TileChangeAnalyzer::checkMembers(ChangedFeature2D* changed, RelationPtr relation)
{
    assert(pastMembers_.empty());
    assert(futureMembers_.empty());

    if (changed->id() == 1988827)
    {
        LOGS << "TileChangeAnalyzer: Checking members of " << changed->typedId();
    }

    bool membersChanged = false;

    for(CFeatureStub* memberStub: changed->members())
    {
        CFeature* member = memberStub->get();
        futureMembers_.insert(member->typedId());
    }

    CFeatureStub** pFutureMember = changed->members().data();
    CFeature::Role* pFutureRole = changed->roles().data();
    int pastMemberCount = 0;
    MemberIterator iter(model_.store(), relation.bodyptr());
    for(;;)
    {
        FeaturePtr member = iter.next();
        if(member.isNull()) break;
        pastMemberCount++;
        TypedFeatureId typedId = member.typedId();
        CRef ref;
        if(iter.isForeign())
        {
            ref = CRef::ofExported(iter.tip(), iter.tex());
        }
        else
        {
            ref = refOfLocal(member);
        }
        pastMembers_.insert(typedId);
        CFeature* f = model_.peekFeature(typedId);
        if(f)
        {
            if(member.hasNorthwestTwin())   [[unlikely]]
            {
                f->setRefSE(ref);
            }
            else
            {
                f->setRef(ref);
            }
        }
        if (typedId.isNode())
        {
            // LOGS << "- Relation member: " << typedId << ", ref: " << ref;
        }

        if(!futureMembers_.contains(typedId))
        {
            addAction<MembershipChange::Removed>(typedId,
                ref, member.hasNorthwestTwin(), changed);
            futureMembers_.insert(typedId);
                // (so we only generate an action once)
        }

        if(pastMemberCount > changed->memberCount()) [[unlikely]]
        {
            membersChanged = true;
        }
        else
        {
            CFeature* futureMember = (*pFutureMember)->get();
            if(typedId != futureMember->typedId() ||
                iter.currentRole() != model_.getRoleString(*pFutureRole))
            {
                // TODO: role check could be more efficient
                membersChanged = true;
            }
            pFutureMember++;
            pFutureRole++;
        }
    }

    membersChanged |= changed->memberCount() > pastMemberCount;

    bool hasChildRelations = false;
    for(CFeatureStub* memberStub: changed->members())
    {
        CFeature* member = memberStub->get();
        TypedFeatureId typedId = member->typedId();
        if(!pastMembers_.contains(typedId))
        {
            if (typedId == TypedFeatureId::ofNode(2422945180))
            {
                LOGS << "TileChangeAnalyzer: Adding " << typedId << " to "
                    << changed->typedId();
            }
            // TODO: We could store pointer to the feature,
            //  avoiding lookup by typedId
            addAction<MembershipChange::Added>(typedId, changed);
            pastMembers_.insert(typedId);       // (so we only generate an action once)
        }
        hasChildRelations |= typedId.isRelation();
    }

    ChangeFlags flagsToClear = membersChanged ? ChangeFlags::NONE : ChangeFlags::MEMBERS_CHANGED;
    changed->clearFlags(flagsToClear);
    changed->addFlags(hasChildRelations ?
        ChangeFlags::WILL_BE_SUPER_RELATION : ChangeFlags::NONE);
    // TODO: Move super-relation detection to ChangeReader?
    //  Currently duplicated in ChangeModel::addNewRelationMemberships()

    pastMembers_.clear();
    futureMembers_.clear();
}

template<typename Action, typename... Args>
void TileChangeAnalyzer::addAction(Args&&... args)
{
    ChangeAction* a = arena_.create<Action>(std::forward<Args>(args)...);
    a->setNext(actions_);
    actions_ = a;
}

void TileChangeAnalyzer::applyActions()
{
    ChangeAction* a = actions_;
    while(a)
    {
        ChangeAction* next = a->next();
            // stash next because apply() may change it due to re-chaining
        a->apply(model_);
        a = next;
    }
    actions_ = nullptr;
}

void TileChangeAnalyzer::readExports()
{
    assert(exports_.empty());
    DataPtr ppExports = pTile_.ptr() + TileConstants::EXPORTS_OFS;
    int exportsRelPtr = ppExports.getInt();
    if(exportsRelPtr == 0) return;
    DataPtr pExports = ppExports + exportsRelPtr;
    int count = (pExports-4).getInt();
    exports_.reserve(count);
    for(int i=0; i<count; i++)
    {
        int featureRelPtr = pExports.getInt();
        if(featureRelPtr)
        {
            int32_t handle = static_cast<int32_t>(pExports.ptr() + featureRelPtr - pTile_.ptr());
            exports_[handle] = i;
        }
        pExports += 4;
    }
}

void TileChangeAnalyzer::analyze(Tip tip, Tile tile, TilePtr pTile)
{
    // LOGS << "Analyzing " << tip;
    tip_ = tip;
    tileMaxX_ = tile.rightX();
    tileMinY_ = tile.bottomY();
    pTile_ = pTile;
    readExports();
    readTileFeatures(pTile);
    exports_.clear();
}