// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangeManager.h"
#include <clarisma/io/FilePath.h>
#include <clarisma/util/Pointers.h>
#include <geodesk/feature/ParentRelationIterator.h>
#include <geodesk/query/ParentWaysQuery.h>
#include "change/model/ChangeModelDumper.h"
#include "change/model/ChangedTile.h"

// TODO: When do we process the membership changes of members
//  of a deleted relation? ==> during scan in TCA

// TODO:
//  We must process all geometrically changed relations *before*
//  non-geometrically changed relations, because a geometric change in a child relation
//  may turn a non-geometrically changed relation into a geometrically changed relation;
//  but at this point, we may have already processed that relation (because we don't
//  iterate and process members unless a relation has geometry changes)
//  Hence we must defer all relations that don't have geometry changes, until
//  all relations with geometric changes have been processed
//  Caution! This may deadlock if a geom-changed rel has a non-geom-changed rel
//   as a member. In that case, we must not process the child -- we simply take its
//   past bounds

// TODO: Avoid pushing unchanged features to tiles

// TODO: When do we check whether a feature loses its TEX as a result
//  of being dropped from a relation?


void ChangeManager::preProcessRelations()
{
    // Add the memberships of newly-created relations
    // to their members (We need to do this *before*
    // we process the members themselves)

    HashSet<TypedFeatureId> memberSet;
    ChangedFeature2D* rel = model_.changedRelations().first();
    while (rel)
    {
        if(rel->isChangedExplicitly()) [[likely]]
        {
            if (rel->ref() == CRef::UNKNOWN)
            {
                // TODO: need to check if both refs are unknown,
                //  because we may have only found the SE twin of
                //  a twin-tile relation

                // If a relation is changed explicitly and
                // it has not been found, this means it has
                // been newly created; we need to add memberships
                // for all its members (for existing relations,
                // TileChangeAnalyzer will perform this step)
                bool hasChildRelations = false;
                for (CFeatureStub* memberStub : rel->members())
                {
                    // TODO: We could avoid the lookup by typedId
                    TypedFeatureId memberId = memberStub->typedId();
                    auto result = memberSet.insert(memberId);
                    if (result.second)  // actually inserted
                    {
                        // Only add a single membership, even if member
                        // appears multiple times in same relation

                        ChangedFeatureBase* member = model_.getChanged(memberId);
                        model_.addMembership(member, rel);
                    }
                    hasChildRelations |= memberId.isRelation();
                }
                memberSet.clear();
                rel->addFlags(hasChildRelations ?
                    ChangeFlags::WILL_BE_SUPER_RELATION : ChangeFlags::NONE);
                // TODO: Move super-relation detection to ChangeReader?
                //  Currently duplicated in TileChangeAnalyzer::checkMembers()
            }
        }
        rel = rel->next();
    }

    // Cascade geometry changes of relations to any of their parent
    // relations (We can't consolidate this with the previous step,
    // because we need to have the membership changes, since we're
    // retrieving relation tables (into which any membership changes
    // are merged)
    rel = model_.changedRelations().first();
    while (rel)
    {
        if(rel->is(ChangeFlags::GEOMETRY_CHANGED))
        {
            model_.memberGeometryChanged(rel);
        }
        rel = rel->next();
    }
}

void ChangeManager::preProcess()
{
    LOGS << "Pre-processing changes...";
    preProcessRelations();
}

void ChangeManager::process()
{
    LOGS << "Processing changes...";

    // Nodes will need to be processed in a potential second turn,
    // because there may be implicitly changed nodes (added to a
    // relation) that haven't been found during the initial search
    processNodes();
    processWays();
    processRelations();

    // TODO: Inform Updater to perform secondary search
}

void ChangeManager::postProcess()
{
    LOGS << "Processing changes...";

    model_.determineTexLosers();

    // TODO: process nodes, ways, relations whose reltables need to be
    //  updated because their parent relations have moved tiles
#ifdef GOL_DIAGNOSTICS
    if (Console::verbosity() >= Console::Verbosity::DEBUG)
    {
        ChangeModelDumper dumper(model_);
        std::string dumpPath(FilePath::withoutExtension(
            model_.store()->fileName()));
        dumpPath += "-change-model.txt";
        dumper.dump(dumpPath.c_str());
    }
#endif

    LOGS << "Post-processed changes.";
    LOGS << changedTileCount() << " tiles changed.";
}


void ChangeManager::processNodes()
{
    LinkedStack nodes(std::move(model_.changedNodes()));
    while(!nodes.isEmpty())
    {
        ChangedNode* node = nodes.pop();
        processNode(node);
    }
}


void ChangeManager::processWays()
{
    LinkedStack ways(std::move(model_.changedWays()));
    while(!ways.isEmpty())
    {
        ChangedFeature2D* way = ways.pop();
        processWay(way);
    }
}


void ChangeManager::processRelations()
{
    // We need to move the relations into a temporary
    // list, because the processing of child relations
    // causes a relation to be moved to another stack
    // while it is still in out local stack
    // TODO: Improve this

    std::vector<ChangedFeature2D*> relationList;
    LinkedStack relations(std::move(model_.changedRelations()));
    while(!relations.isEmpty())
    {
        relationList.push_back(relations.pop());
    }

    for (ChangedFeature2D* rel : relationList)
    {
        if(testAny(rel->flags(),
            ChangeFlags::PROCESSED |
            ChangeFlags::RELATION_DEFERRED))
        {
            continue;
        }
        processRelation(rel);
    }
}

// TODO: Check if shared_location flag changes; if so, set FLAGS_CHANGED

// TODO: if an anon node becomes a feature (get tags, added to relation,
//  becomes a duplicate), its parent ways will need to be updated
//  (node-table changed = technical change);
//  likewise, feature node to anon (loses tags and removed from all rels,
//  or duplicate becomes unique), need to update parent ways

// TODO: Now we have chicken/egg problem: ways need all processed nodes,
//  but an implicit delete of a way (invalid, or missing nodes) may
//  turn its nodes into orphans
//  Solution: never implicitly delete a way, try to fix it instead
//   by removing missing nodes, or interpolating
//   Only discard a way if *all* of its nodes are missing
//   This differs from gol build, which currently discards all ways
//   with *any* missing nodes

// TODO: when/how do we determine if a node's geometry changed?

// TODO: Cascade node move to parent relations

// TODO: If feature status change, need to implicitly change ways
//  (their node table must be updated)

// TODO: what happens if a node moves AND is deleted?

void ChangeManager::processNode(ChangedNode* node)
{
    if(node->id() == 3)
    {
        LOGS << "Processing node/" << node->id()
            << ", version: " << node->version()
            << ", ref: " << node->ref()
            << ", flags: " << static_cast<uint32_t>(node->flags());
    }
    CRef pastRef = node->ref();
    Tip pastTip = pastRef.tip();
    NodePtr pastNode = node->getFeature(store());
    uint32_t pastFeatureFlags = 0;
    if (!pastNode.isNull())
    {
        pastFeatureFlags = pastNode.flags();
    }

    // TODO: Process case where a feature node is added to a way
    //  for the first time, requiring its waynode_flag to be set
    //  (can be an implicit change without any other changes to the node)
    //  Adding an orphan node to a way revokes its orphan status
    //  and may cause it to become anonymous

    processMembershipChanges(node);

    if(node->isDeleted())
    {
        if(!pastTip.isNull())
        {
            // TODO: A deleted node always loses any TEX
            model_.getChangedTile(pastTip)->deletedNodes().push(node);
        }
        node->setRef(CRef::MISSING);
        node->addFlags(ChangeFlags::PROCESSED);
        return;

        // TODO: If node was a feature, a delete has to be modify
        //  any parent ways & relations via cascade, because we
        //  cannot be guaranteed that the node has been removed
        //  from those parents (we cannot assume that the osc
        //  respects referential integrity)
    }

    if (node->xy().isNull())    [[unlikely]]
    {
        // TODO: Can we just avoid this scenario that CRef is
        //  set but x/y is not, so we don't have to fix it here?

        if (!pastNode.isNull())
        {
            node->setXY(pastNode.xy());
        }
        if (node->xy().isNull())
        {
            node->setRef(CRef::MISSING);
            return;
        }
    }

    ChangeFlags changeFlags = node->flags();
    bool willHaveTags;
        // TODO: consider exception nodes (orphan, duplicate) and disregard their tags
        //  but we only care whether node is a feature (orphans & dupes are features)
    if (test(changeFlags, ChangeFlags::TAGS_CHANGED))
    {
        willHaveTags = node->tagTable() != &CTagTable::EMPTY;
    }
    else
    {
        if (pastNode.isNull())
        {
            willHaveTags = false;
        }
        else
        {
            willHaveTags = !pastNode.tags().isEmpty() &&
                (pastFeatureFlags & FeatureFlags::EXCEPTION_NODE) == 0;
            // An exception node (orphan or duplicate) has synthetic tags;
            // these don't count as "having tags"
        }
    }

    bool willBeRelationMember;
    if(testAny(changeFlags, ChangeFlags::ADDED_TO_RELATION |
        ChangeFlags::REMOVED_FROM_RELATION))
    {
        willBeRelationMember = node->peekParentRelations() != nullptr;
    }
    else
    {
        willBeRelationMember = pastNode.isNull() ? false : pastNode.isRelationMember();
    }

    bool hasBelongedToWay = pastRef == CRef::ANONYMOUS_NODE ||
        (pastFeatureFlags & FeatureFlags::WAYNODE);
    bool willBelongToWay = node->isFutureWaynode();
    if (!willBelongToWay)
    {
        if (test(changeFlags, ChangeFlags::REMOVED_FROM_WAY))
        {
            // If the node has been removed from a way, we now need
            // to check if it still belongs to at least one way
            // We assume the answer is "no"
            willBelongToWay = false;
            ParentWaysQuery query(store(), node->xy(), pastNode);
            for (;;)
            {
                WayPtr way = query.next();
                if (way.isNull()) break;
                CFeature* feature = model_.peekFeature(TypedFeatureId::ofWay(way.id()));
                if (feature == nullptr || !feature->isChanged())
                {
                    // If the anon node belonged to a way that is not
                    // tracked by the model or hasn't changed, we know
                    // it still belongs to that way
                    willBelongToWay = true;
                    break;
                }
                ChangedFeatureBase* changed = ChangedFeatureBase::cast(feature);
                if (!changed->isChangedExplicitly() && !changed->isDeleted())
                {
                    // The way was changed, but not explicitly (hence no
                    // change in waynodes), and it hasn't been deleted
                    // (remember, deletions can also be implicit!);
                    // i.e. the way only changed geometry, which means
                    // it will continue to include the node --> not orphan
                    willBelongToWay = true;
                    break;
                }
            }
        }
        else
        {
            willBelongToWay = hasBelongedToWay;
        }
    }

    changeFlags |= willBelongToWay ? ChangeFlags::FLAGGED_WAYNODE : ChangeFlags::NONE;
    changeFlags |= (hasBelongedToWay != willBelongToWay) ?
        ChangeFlags::FLAGS_CHANGED : ChangeFlags::NONE;

    // TODO: duplicate

    bool wasCoincident = pastFeatureFlags & FeatureFlags::SHARED_LOCATION;
    bool wasDuplicate = (pastFeatureFlags &
        (FeatureFlags::SHARED_LOCATION | FeatureFlags::EXCEPTION_NODE)) ==
        (FeatureFlags::SHARED_LOCATION | FeatureFlags::EXCEPTION_NODE);
    bool willBeCoincident = test(changeFlags, ChangeFlags::FLAGGED_SHARED_LOCATION);

    if (wasCoincident) [[unlikely]]
    {
        // If a coincident node moved, we need to check if only
        // one node remains at its past location -- if so, that
        // node loses its SHARED_LOCATION flag (and may lose its
        // feature status if it is untagged, does not belong to
        // a relation, and is not an orphan).

        // If a coincident node has not moved, we need to still
        // check if all other nodes have moved from its location,
        // causing it to be the sole node that location

        assert(!pastTip.isNull());
        ChangedNode* uniqueNode = findUniqueLocationNode(pastTip, pastNode.xy());
        if (!willBeCoincident)
        {
            if (test(changeFlags, ChangeFlags::GEOMETRY_CHANGED))
            {
                // If the formerly coincident node moved, and it is
                // not coincident at its new location, it loses its
                // SHARED_LOCATION flag (already cleared, but we
                // need to mark the flag change so the node will be
                // updated)
                changeFlags |= ChangeFlags::FLAGS_CHANGED;
            }
            else
            {
                // If the node is not explicitly marked as being coincident
                // in the future, it will stay coincident if it is not
                // the unique node at its location
                if (uniqueNode != node)
                {
                    willBeCoincident = true;
                }
                else
                {
                    // SHARED_LOCATION already cleared, mark the flag change
                    changeFlags |= ChangeFlags::FLAGS_CHANGED;
                }
            }
        }
    }

    bool willBeDuplicate = willBeCoincident && !willHaveTags;

    // Determine orphan status

    bool willBeOrphan = !willHaveTags && !willBeRelationMember && !willBelongToWay;
    bool wasOrphan = (pastFeatureFlags & (FeatureFlags::EXCEPTION_NODE |
        FeatureFlags::WAYNODE | FeatureFlags::RELATION_MEMBER)) == FeatureFlags::EXCEPTION_NODE;

    changeFlags |= (willBeDuplicate || willBeOrphan) ?
        ChangeFlags::FLAGGED_EXCEPTION_NODE : ChangeFlags::NONE;

    if (wasDuplicate != willBeDuplicate || wasOrphan != willBeOrphan) [[unlikely]]
    {
        changeFlags |= ChangeFlags::FLAGS_CHANGED;
        if (willBeOrphan || willBeDuplicate)
        {
            node->setTagTable(getExceptionNodeTags(willBeDuplicate, willBeOrphan));
            changeFlags |= ChangeFlags::TAGS_CHANGED;
        }
    }

    bool willBeFeature = willHaveTags | willBeRelationMember |
        willBeOrphan | willBeDuplicate;

    Tip futureTip = tileCatalog_.tipOfCoordinateSlow(node->xy());
    futureTip = willBeFeature ? futureTip : Tip();

    if(futureTip != pastTip)
    {
        if(!pastTip.isNull())
        {
            ChangedTile* pastTile = model_.getChangedTile(pastTip);
            pastTile->deletedNodes().push(model_.copy(node));
            // LOGS << "Deleted " << node->typedId() <<", future TIP = " << futureTip;
            // TODO: drop TEX, if any
        }
        if(!futureTip.isNull())
        {
            node->setRef(CRef::ofNew(futureTip));
            changeFlags |= ChangeFlags::NEW_TO_NORTHWEST | ChangeFlags::TILES_CHANGED;
            // If node moves to another tile, we will need to write its tags
            //  and rels
            if (!node->tagTable())
            {
                const CTagTable* tags = pastRef.tip().isNull() ?
                    &CTagTable::EMPTY : model_.getTagTable(pastRef);
                assert(tags);
                node->setTagTable(tags);
            }
            if (!node->peekParentRelations())
            {
                node->setParentRelations(model_.getRelationTable(pastRef));
            }
        }
        else
        {
            if(node->isFutureWaynode())
            {
                node->setRef(CRef::ANONYMOUS_NODE);
            }
        }
    }
    if(!futureTip.isNull())
    {
        ChangedTile* futureTile = model_.getChangedTile(futureTip);
        futureTile->changedNodes().push(node);
        if (test(changeFlags, ChangeFlags::GEOMETRY_CHANGED))
        {
            // If node is (and was) a feature node and has moved,
            // its parent relations (if any) may implicitly change
            // (If node is added to a relation for the first time,
            // we won't need to call this method, since its parent
            // relations by definition already explicitly change)
            // model_.cascadeMemberChange(pastNode, node);
            model_.memberGeometryChanged(node);
        }
    }
    else
    {
        // TODO: We need to prevent a changed node that is not a feature
        //  from being written into the TES
        //  There is probably a better way to do this
        //  --> If we don't push it to the changedNodes stack,
        //      why would ChangeWriter write it to the TES??
        //      (because it is referenced by a way -- but check)
        changeFlags &= ~(ChangeFlags::TAGS_CHANGED | ChangeFlags::GEOMETRY_CHANGED);
        node->setRef(node->ref() == CRef::MISSING ?
            CRef::MISSING : CRef::ANONYMOUS_NODE);
    }

    bool wasFeature = !pastNode.isNull();
    if (willBeFeature != wasFeature)    [[unlikely]]
    {
        // If a node's feature status has changed, all ways that
        // contain this node need to update their node tables

        if (!test(changeFlags, ChangeFlags::GEOMETRY_CHANGED))
        {
            // Only do this if the node hasn't moved (for nodes that
            // moved, the TileChangeAnalyzer has already marked their
            // implicitly changed parent ways

            if (willBeFeature || (pastFeatureFlags & FeatureFlags::WAYNODE) != 0)
            {
                // Only do this if an anonymous node (which is always a waynode)
                // turn feature node, or a waynode-flagged feature node turns
                // anonymous

                wayNodeFeatureStatusChanged(node->xy(), pastNode);
            }
        }
    }
    changeFlags |= ChangeFlags::PROCESSED;
    node->setFlags(changeFlags);
}

// TODO: move to ChangeModel
void ChangeManager::wayNodeFeatureStatusChanged(Coordinate xy, NodePtr node)
{
    ParentWaysQuery query(store(), xy, node);
    for (;;)
    {
        WayPtr way = query.next();
        if (way.isNull()) break;
        ChangedFeature2D* changedWay =
            model_.getChangedFeature2D(FeatureType::WAY, way.id());
        CRef ref = getRef(way);
        if (!way.hasNorthwestTwin()) [[likely]]
        {
            changedWay->offerRef(ref);
        }
        else
        {
            changedWay->offerRefSE(ref);
        }
        // TODO: need to mark the way?
    }
}


void ChangeManager::addDeleted(Tip tip, ChangedFeatureStub* feature)
{
    assert(feature->type() != FeatureType::NODE);
    ChangedTile* tile = model_.getChangedTile(tip);
    (feature->type() == FeatureType::WAY ?
        tile->deletedWays() : tile->deletedRelations()).push(feature);
}

void ChangeManager::processDeletedFeature(ChangedFeature2D* deleted)
{
    Tip tip = deleted->ref().tip();
    if(!tip.isNull()) addDeleted(tip, deleted);
        // TIP could be null if feature does not exist
        // (already deleted)
    tip = deleted->refSE().tip();
    if(!tip.isNull()) addDeleted(tip, model_.copy(deleted));
    deleted->setRef(CRef::MISSING);
    deleted->setRefSE(CRef::MISSING);
    deleted->addFlags(ChangeFlags::PROCESSED);
}


void ChangeManager::processMembershipChanges(ChangedFeatureBase* feature)
{
    // TODO: Do we need to guard against the reltable already
    //  being loaded? (Changes and actual table are unioned)

    const MembershipChange* changes = feature->membershipChanges();
    if (changes)    [[unlikely]]
    {
        CRef ref = feature->ref();
        if (!ref.canGetFeature() && feature->type() != FeatureType::NODE)
        {
            ref = feature->refSE();
        }
        feature->setParentRelations(model_.getRelationTable(ref, changes));
    }
}

// TODO: What if way refers to deleted node?? (pathological)
// TODO: We must always scan the way's nodes, to
//  a) mark whether way will have feature nodes
//  b) defer way if a node has not been processed
//     (This can happen for implicitly changed nodes, e.g.
//      node added to a relation -- which may upgrade it
//      from anon to feature, but it has been deferred
//      because its location is not yet known)
void ChangeManager::processWay(ChangedFeature2D* way)
{
    if(way->id() == 393548000 || way->id() == 215088731)
    {
        LOGS << "Processing " << way->typedId();
    }

    if(way->isDeleted())
    {
        // TODO: Need to normalize refs, because under the new search
        //  model, we may only have one ref of a twin-tile feature
        processDeletedFeature(way);
        return;
    }

    // TODO: need to do this even for explicitly changed ways,
    //  because we may only be searching the NW tiles of twin-tile ways
    if (!way->isChangedExplicitly())
    {
        if (normalizeRefs(way) < 1) return;
        // TODO: For both unknown and missing, we need to push the way back
        //  onto the stack of changed ways
        // TODO: but when can that actually happen??
    }

    bool defer = false;
    Box newBounds;

    // assert(way->memberCount() >= 2);
    // TODO: We must decide what to do with pathological ways
    //  (less than 2 nodes) -- ignore the change, or delete the way?

    bool willHaveFeatureNodes = false;
    bool missingNodes = false;
    for(CFeatureStub* nodeStub : way->members())
    {
        // TODO: We could deal with potential waynode flag change
        //  of nodes here, which would alleviate hte need for
        //  ChangeModel::prepareWays() and the check in the TileChangeAnalyzer
        //  but still need to deal with nodes that become orphans

        CFeature* node = nodeStub->get();
        CRef ref = node->ref();
        if (ref.isUnknownOrMissing())   [[unlikely]]
        {
            if(ref == CRef::MISSING || memberSearchCompleted_)
            {
                node->setRef(CRef::MISSING);
                missingNodes = true;
            }
            else
            {
                // TODO: look up node in index, issue search instruction
                defer = true;
            }
        }
        else
        {
            newBounds.expandToInclude(node->xy());
        }
        willHaveFeatureNodes |= !ref.tip().isNull();
        if (node->isChanged())
        {
            defer |= !ChangedNode::cast(node)->is(ChangeFlags::PROCESSED);
        }
    }
    way->addFlags(willHaveFeatureNodes ?
        ChangeFlags::WAY_WILL_HAVE_FEATURE_NODES :
        ChangeFlags::NONE);

    if(defer)
    {
        model_.changedWays().push(way);
        return;
    }

    // if (newBounds.isEmpty())    [[unlikely]]
    if (missingNodes)    [[unlikely]]
    {
        // Was: If all nodes of a way are missing, throw it away
        // If *any* nodes of a way are missing, throw it away
        // TODO: We can likely interpolate missing nodes if
        //  at least one node of a way is missing;
        //  this allows us to salvage the way if the node
        //  is part of a later update
        processDeletedFeature(way);
        return;
    }

    processMembershipChanges(way);
    if (way->is(ChangeFlags::GEOMETRY_CHANGED))
    {
        updateBounds(way, newBounds);
        model_.memberGeometryChanged(way);
    }
    assignToTiles(way);
    bool membersChanged = false;
    if (willHaveFeatureNodes)
    {
        // If a way may have new nodes, or its tiles changed,
        // we need to check if its feature nodes gain or lose
        // their TEX (a check is also needed if the individual
        // node has moved tiles)

        bool texCheckNeeded = way->isAny(
            ChangeFlags::TILES_CHANGED |
            ChangeFlags::WAYNODE_IDS_CHANGED);

        WayPtr pastWay(way->getFeature(store()));
        int pastWayFlags;
        DataPtr pastWayBody;
        if (pastWay.isNull())
        {
            membersChanged = true;
            pastWayFlags = 0;
        }
        else
        {
            pastWayFlags = pastWay.flags();
            pastWayBody = pastWay.bodyptr();
        }

        // TODO: Do we need to consult the old node table?

        FeatureNodeIterator iter(store(), pastWayBody,
            pastWayFlags, store()->borrowAllMatcher(), nullptr);
        for(CFeatureStub* nodeStub : way->members())
        {
            CFeature* node = nodeStub->get();
            Tip nodeTip = node->ref().tip();
            if(!nodeTip.isNull())
            {
                // Node is a feature node
                bool nodeChangedTiles = false;
                if(node->isChanged())
                {
                    nodeChangedTiles = ChangedNode::cast(node)
                        ->is(ChangeFlags::TILES_CHANGED);
                    membersChanged |= nodeChangedTiles;
                    // If a feature node of a way has moved tiles, we always
                    // have to write the node table
                }
                NodePtr pastNode = iter.next();
                if(pastNode.isNull())
                {
                    membersChanged = true;
                }
                else if(pastNode.id() != node->id())
                {
                    membersChanged = true;
                }

                if (texCheckNeeded || nodeChangedTiles)
                {
                    // If the way or the way's node have changed tile,
                    // or the way may have gained a node, we need to
                    // check if the node becomes foreign or
                    // local, hence gaining a TEX or losing its TEX

                    // TODO: mark a node if it has been added to a way?

                    bool nodeWillBeForeign = node->isFutureForeign();
                    if (!nodeWillBeForeign)
                    {
                        // Node has not been marked as foreign yet

                        nodeWillBeForeign = nodeTip != way->ref().tip();
                        Tip wayTipSE = way->refSE().tip();
                        nodeWillBeForeign |= !wayTipSE.isNull();
                        // (nodes of dual-tile ways by definition
                        // are always foreign)

                        checkExport(node, nodeWillBeForeign);
                    }
                }
            }
        }
        membersChanged |= !iter.next().isNull();
        // If the way had additional nodes in the past,
        // we'll need to update its node table
    }
    way->addFlags(membersChanged ?
        (ChangeFlags::MEMBERS_CHANGED | ChangeFlags::PROCESSED):
        ChangeFlags::PROCESSED);
}

/// Past bounds must be set
///
/// @param changed
/// @return  1   if at least one ref has been resolved
///          0   if feature is missing
///         -1   if feature refs are unknown (search required)
///
// TODO: This may be wrong, because it looks up the tile of
//  the topLeft/bottomRight coordinate; in reality, the
//  true twin-tile may be at a lower zoom level, we need to
//  look at the bounds of the feature to determine its level
int ChangeManager::normalizeRefs(CFeature* feature)
{
    // For twin-tile features, we may have only one of the twins.
    // If the one ref is MISSING, we will set it to either UNRESOLVED
    // (i.e. we know the TIP, but don't have its offset or TEX),
    // or SINGLE_TILE (SE part only)

    assert(feature->type() != FeatureType::NODE);
    CRef ref = feature->ref();
    Tip tip = ref.tip();
    if(!tip.isNull())   [[likely]]
    {
        CRef refSE = feature->refSE();
        if (refSE == CRef::SINGLE_TILE) [[likely]]
        {
            return 1;
        }
        if (feature->refSE().tip().isNull())
        {
            Box pastBounds = ref.getFeature(store()).bounds();
            Box tileBounds = tileCatalog_.tileOfTip(tip).bounds();
            refSE = CRef::SINGLE_TILE;
            if(pastBounds.maxX() > tileBounds.maxX() ||
                pastBounds.minY() < tileBounds.minY())
            {
                // The feature's bounds extend past the right or
                // bottom edge of its NW tile, which means it has
                // a SE tile

                TilePair tp = tileCatalog_.tilePair(pastBounds);
                assert(tp.first().tip() == tip);
                assert(tp.hasSecond());
                refSE = CRef::ofUnresolved(tileCatalog_.tipOfTile(tp.second()));
            }
            feature->setRefSE(refSE);
        }
    }
    else
    {
        ref = feature->refSE();
        tip = ref.tip();
        if (tip.isNull())
        {
            if (memberSearchCompleted_)
            {
                feature->setRef(CRef::MISSING);
                return 0;
            }

            // TODO: Look up feature in index, issue
            //  search request
            return -1;
        }

        Box pastBounds = ref.getFeature(store()).bounds();
        Box tileBounds = tileCatalog_.tileOfTip(tip).bounds();
        assert(pastBounds.minX() < tileBounds.minX() ||
            pastBounds.maxY() > tileBounds.maxY());

        // If we only have an SE ref, the feature *must* have
        // a NW tile (TODO: make these runtime checks instead
        //  of asserts -- if these constraints are violated,
        //  this means the GOL is corrupt)

        TilePair tp = tileCatalog_.tilePair(pastBounds);
        assert(tp.second().tip() == tip);
        assert(tp.hasSecond());
        ref = CRef::ofUnresolved(tileCatalog_.tipOfTile(tp.first()));
        feature->setRef(ref);
    }
    return 1;
}

CRef ChangeManager::deduceTwinRef(CRef ref) const
{
    FeaturePtr feature = ref.getFeature(store());
    assert(!feature.isNull());
    assert(!feature.isNode());
    Box tileBounds = tileCatalog_.tileOfTip(ref.tip()).bounds();
    Box bounds = feature.bounds();

    if(bounds.maxX() > tileBounds.maxX() || bounds.minY() < tileBounds.minY())
    {
        // The feature's bounds extend past the right or
        // bottom edge of its NW tile, which means it has
        // a SE tile
        return CRef::ofUnresolved(
            tileCatalog_.tipOfCoordinateSlow(bounds.bottomRight()));
    }
    if(bounds.minX() < tileBounds.minX() || bounds.maxY() > tileBounds.maxY())
    {
        // The feature's bounds extend past the left or
        // top edge of its SE tile, which means it has
        // a NW tile
        return CRef::ofUnresolved(
            tileCatalog_.tipOfCoordinateSlow(
                bounds.topLeft()));
    }
    return CRef::SINGLE_TILE;
}


void ChangeManager::updateBounds(ChangedFeature2D* feature, const Box& bounds)
{
    // TODO: assumes future->bounds_ has been set to past bounds
    assert(feature->type() != FeatureType::NODE);
    if (bounds.isEmpty())
    {
        LOGS << feature->typedId() << ": bounds empty";
    }
    assert(!bounds.isEmpty());
    if(feature->bounds() != bounds)
    {
        feature->setBounds(bounds);
        feature->addFlags(ChangeFlags::BOUNDS_CHANGED);

        // TODO: Need to ensure this works for relations
        //  We need to process all geometrically changed relations
        //  before non-geometrically changed rels!
        // model_.cascadeMemberChange(feature->getFeature(store()), feature);

        // We don't cascade bounds changes; we let processWay
        //  cascade geometry changes instead

        // If bounds changed, tiles may change


        TilePair futureTiles(tileCatalog_.tileOfCoordinateSlow(
            bounds.bottomLeft()));
        futureTiles += tileCatalog_.tileOfCoordinateSlow(bounds.topRight());
        // TODO: this is sub-optimal
        futureTiles = tileCatalog_.normalizedTilePair(futureTiles);
        updateTiles(feature, futureTiles);
    }
}


void ChangeManager::updateTiles(ChangedFeature2D* feature, TilePair futureTiles)
{
    if (feature->typedId() == TypedFeatureId::ofWay(208248639))
    {
        LOGS << "Updating tiles of " << feature->typedId();
    }
    ChangeFlags tileChanges = ChangeFlags::NONE;
    CRef pastRefNW = feature->ref();
    CRef pastRefSE = feature->refSE();
    Tip pastTipNW = pastRefNW.tip();
    Tip pastTipSE = pastRefSE.tip();
    Tip futureTipNW = tileCatalog_.tipOfTile(futureTiles.first());
    Tip futureTipSE = futureTiles.hasSecond() ?
        tileCatalog_.tipOfTile(futureTiles.second()) : Tip();
    assert(pastTipNW != pastTipSE || pastTipNW.isNull());
    assert(futureTipNW != futureTipSE);
    assert(!futureTipNW.isNull());

    if (pastTipNW != futureTipNW)
    {
        tileChanges |= ChangeFlags::TILES_CHANGED;
        if (futureTipNW != pastTipSE)
        {
            tileChanges |= ChangeFlags::NEW_TO_NORTHWEST;
            if (pastTipNW != futureTipSE)
            {
                // TODO: remove from past NW tile
            }
            feature->setRef(CRef::ofNew(futureTipNW));
        }
        else
        {
            // Set SE tile as new NW tile
            // (feature simply moved SE)
            feature->setRef(pastRefSE);
        }
    }

    if (pastTipSE != futureTipSE)
    {
        tileChanges |= ChangeFlags::TILES_CHANGED;
        if (futureTipSE != pastTipNW)
        {
            if (!pastTipSE.isNull())
            {
                // TODO: remove from past SE tile
            }
            if (futureTipSE.isNull())
            {
                feature->setRefSE(CRef::SINGLE_TILE);
            }
            else
            {
                feature->setRefSE(CRef::ofNew(futureTipSE));
                tileChanges |= ChangeFlags::NEW_TO_SOUTHEAST;
            }
        }
        else
        {
            // Set NW tile as new SE tile
            // (feature simply moved NW)
            feature->setRefSE(pastRefNW);
        }
    }
    if (futureTipSE.isNull())
    {
        feature->setRefSE(CRef::SINGLE_TILE);
    }
    feature->addFlags(tileChanges);

    // TODO: Does it make sense to mark a feature as NEW (a common case)
    //  to skip these checks?
    if (testAny(tileChanges, ChangeFlags::NEW_TO_NORTHWEST |
        ChangeFlags::NEW_TO_SOUTHEAST))
    {
        CRef sourceRef = pastRefNW;
        if (!sourceRef.canGetFeature())
        {
            sourceRef = pastRefSE;
        }
        if (!feature->tagTable() && sourceRef.canGetFeature())
        {
            feature->setTagTable(model_.getTagTable(sourceRef));
        }
        if (!feature->is(ChangeFlags::RELTABLE_LOADED))
        {
            feature->setParentRelations(model_.getRelationTable(sourceRef));
        }
    }
}

void ChangeManager::cascadeNodeCoordinateChange(NodePtr node, Coordinate futureXY)
{
    if(!node.isRelationMember()) return;
    Coordinate pastXY = node.xy();
    ParentRelationIterator iter(store(), node.relationTableFast(),
        store()->borrowAllMatcher(), nullptr);
    for(;;)
    {
        RelationPtr parent = iter.next();
        if(parent.isNull()) break;
        Box pastParentBounds = parent.bounds();
        if(!pastParentBounds.contains(futureXY) ||
            pastXY.x == pastParentBounds.minX() ||
            pastXY.x == pastParentBounds.maxX() ||
            pastXY.y == pastParentBounds.minY() ||
            pastXY.y == pastParentBounds.maxY())
        {
            // Unless node's future location lies within the parent's
            // past bounds, and the node's past location did not
            // lie on the parent's bounds, the node's location change
            // may cause the parent's bounds to change

            Console::log("Bounds of relation/%lld may change due to "
                "location change of node/%lld", parent.id(), node.id());

            model_.getChangedFeature2D(FeatureType::RELATION, parent.id())
                ->addFlags(ChangeFlags::BOUNDS_CHANGED);
        }
    }
}

void ChangeManager::cascadeBoundsChange(FeaturePtr feature, const Box& futureBounds)
{
    assert(!feature.isNode());
    if(!feature.isRelationMember()) return;
    Box pastBounds = feature.bounds();
    ParentRelationIterator iter(store(), feature.relationTableFast(),
        store()->borrowAllMatcher(), nullptr);
    for(;;)
    {
        RelationPtr parent = iter.next();
        if(parent.isNull()) break;
        Box pastParentBounds = parent.bounds();
        if(!pastParentBounds.containsSimple(futureBounds) ||
            pastBounds.minX() == pastParentBounds.minX() ||
            pastBounds.minY() == pastParentBounds.minY() ||
            pastBounds.maxX() == pastParentBounds.maxX() ||
            pastBounds.maxY() == pastParentBounds.maxY())
        {
            // Unless the member's future bounds lie entirely within the
            // parent's past bounds, and the member's past bounds did not
            // lie on the parent's bounds, the member's bounds change may
            // cause the parent's bounds to change as well

            Console::log("Bounds of relation/%lld may change due to "
                "bounds change of member %s/%lld", parent.id(),
                feature.isWay() ? "way" : "relation", feature.id());

            model_.getChangedFeature2D(FeatureType::RELATION, parent.id())
                ->addFlags(ChangeFlags::BOUNDS_CHANGED);
        }
    }
}

// TODO: What if relation has deleted members?? (pathological)
int ChangeManager::processRelation(ChangedFeature2D* rel) // NOLINT recursive
{
    //LOGS << "Processing " << rel->typedId();
    if(rel->id() == 169101)
    {
        LOGS << "Processing " << rel->typedId();
    }

    if(rel->isDeleted())
    {
        processDeletedFeature(rel);
        return 1;
    }

    // TODO: need to do this even for explicitly changed ways,
    //  because we may only be searching the NW tiles of twin-tile ways
    if (!rel->isChangedExplicitly())
    {
        int result = normalizeRefs(rel);
        if (result < 1)
        {
            if (result == 0)
            {
                rel->addFlags(ChangeFlags::PROCESSED);
            }
            // TODO: For both unknown and missing, we need to push the way back
            //  onto the stack of changed ways
            //  No, only for unknown
            return 0;
        }

    }

    model_.ensureMembersLoaded(rel);
    rel->addFlags(ChangeFlags::RELATION_ATTEMPTED);

    bool hasUnresolvedMembers = false;
    bool memberTilesChanged = false;
    int omittedMembersCount = 0;
    Box bounds;

    // If a relation will be a super-relation, we always process its members,
    // even for a super-relation without geometry changes or member changes,
    // to ensure that child relations are always processed before parents
    // This avoids a situation where a child relation with geom changes
    // is processed after its parents relation without geom changes,
    // which may cause geometry changes to cascade to the parent --
    // but at that point, the parent has already been processed (can't
    // process it twice). This also means we need to implicitly change
    // all unchanged child relations of a changed parent, so the processing
    // can descend to its respective children

    if (rel->isAny(
        ChangeFlags::MEMBERS_CHANGED |
        ChangeFlags::GEOMETRY_CHANGED |
        ChangeFlags::WILL_BE_SUPER_RELATION))
    {
        auto members = rel->members();
        for(int i=0; i<members.size(); i++)
        {
            if(members[i] == nullptr)   [[unlikely]]
            {
                // The member has been determined missing in an
                // earlier attempt, and replaced with null
                omittedMembersCount++;
                continue;
            }
            CFeature* member = members[i]->get();
            FeatureType memberType = member->type();

            if(memberType == FeatureType::RELATION)     [[unlikely]]
            {
                if(rel->id() == 169101 || rel->id() == 17721802)
                {
                    LOGS << "Processing member " << member->typedId() << " of " << rel->typedId();
                }

                // We always upgrade a child relation to "changed",
                // (even if it has o actual changes), in order to allow
                // processing to descend to any of its potential child
                // relations (which may have actual changes), to ensure
                // that child relations are always processed before
                // parent relations

                ChangedFeature2D* memberRel = model_.getChangedFeature2D(member);
                member = memberRel;
                    // so subsequent ops use the ChangedFeature2D, not the stub
                if(memberRel->is(ChangeFlags::RELATION_ATTEMPTED))  [[unlikely]]
                {
                    // TODO: We have a circular reference

                    ConsoleWriter out;
                    out << memberRel->typedId() << ": Reference cycle (referenced from "
                        << rel->typedId() << ")\n";
                    out.flush();

                    assert(false);
                        // TODO: for now -- since we don't break
                        //  refcycles yet
                }
                else if(memberRel->is(ChangeFlags::RELATION_DEFERRED))
                {
                    hasUnresolvedMembers = true;
                    continue;
                }
                else if(!memberRel->is(ChangeFlags::PROCESSED))
                {
                    int res = processRelation(memberRel);
                    // TODO: -1 = refcycle
                    if(res == 0)
                    {
                        hasUnresolvedMembers = true;
                        continue;
                    }
                }
            }

            if(member->ref().isUnknownOrMissing())   [[unlikely]]
            {
                if(memberType != FeatureType::NODE &&
                    !member->refSE().tip().isNull())
                {
                    // If only the SE tile is known, we can deduce
                    // the NW tile
                    member->setRef(deduceTwinRef(member->refSE()));
                }
                else
                {
                    // TODO: No need to issue secondary search for a feature with
                    //  "unknown" ref which has been explicitly changed
                    //  (If it existed, it would have been found, hence it must be new)

                    if(member->ref() == CRef::MISSING || memberSearchCompleted_)
                    {
                        member->setRef(CRef::MISSING);
                        members[i] = nullptr;
                        omittedMembersCount++;
                    }
                    else
                    {
                        // TODO: look up feature in index, issue
                        hasUnresolvedMembers = true;
                    }
                    member = nullptr;
                }
            }
            else
            {
                if(member->type() != FeatureType::NODE &&
                    member->refSE() == CRef::UNKNOWN)
                {
                    /*
                    LOGS << "Deducing SE ref for " << member->typedId() <<
                        " based on NW ref " << member->ref();
                    */
                    member->setRefSE(deduceTwinRef(member->ref()));
                }
            }

            if(member)
            {
                if(memberType == FeatureType::NODE)  [[unlikely]]
                {
                    if (member->isChanged())
                    {
                        memberTilesChanged |= ChangedNode::cast(member)->is(
                            ChangeFlags::TILES_CHANGED);
                    }
                    if (member->xy().isNull()) [[unlikely]]
                    {
                        LOGS << member->typedId() << " (ref "
                            << member->ref() << ") has null coordinate";
                    }
                    assert(!member->xy().isNull());
                    bounds.expandToInclude(member->xy());
                }
                else
                {
                    Box memberBounds;
                    if(member->isChanged())
                    {
                        ChangedFeature2D* member2D = ChangedFeature2D::cast(member);
                        if(!member2D->is(ChangeFlags::PROCESSED))
                        {
                            hasUnresolvedMembers = true;
                            continue;
                        }
                        memberBounds = member2D->bounds();
                        memberTilesChanged |= member2D->is(
                            ChangeFlags::TILES_CHANGED);
                    }
                    if (memberBounds.isEmpty())
                    {
                        memberBounds = member->getFeature(store()).bounds();
                    }
                    bounds.expandToIncludeSimple(memberBounds);
                }
            }
        }
    }

    if(hasUnresolvedMembers)   [[unlikely]]
    {
        rel->addFlags(ChangeFlags::RELATION_DEFERRED);
        rel->clearFlags(ChangeFlags::RELATION_ATTEMPTED);
        model_.changedRelations().push(rel);
        LOGS << "Deferred " << rel->typedId();
        return 0;
    }

    if (omittedMembersCount && omittedMembersCount == rel->memberCount()) [[unlikely]]
    {
        LOGS << rel->typedId() << ": all members missing";
        // Delete relation without any members
        processDeletedFeature(rel);
        rel->clearFlags(ChangeFlags::RELATION_ATTEMPTED);
        return 1;
    }

    if (rel->id() == 17721802)
    {
        LOGS << "Processing membership changes for " << rel->typedId();
    }
    processMembershipChanges(rel);
    if (rel->id() == 17721802)
    {
        if (rel->peekParentRelations())
        {
            LOGS << rel->typedId() << " has "
                << rel->peekParentRelations()->relations().size()
                << "parent relations";
        }
        else
        {
            LOGS << rel->typedId() << " has no parent relations";
        }
    }
    if (rel->isAny(ChangeFlags::MEMBERS_CHANGED | ChangeFlags::GEOMETRY_CHANGED))
    {
        updateBounds(rel, bounds);
        if (memberTilesChanged || rel->isAny(ChangeFlags::TILES_CHANGED |
            ChangeFlags::MEMBERS_CHANGED))
        {
            // If the relation or any of its members changed tiles,
            // or if the relation may have gained members,
            // we need to check for potential TEX gainers/losers

            checkMemberExports(rel);
            rel->addFlags(ChangeFlags::MEMBERS_CHANGED);
        }
    }
    rel->addFlags(ChangeFlags::PROCESSED);
    rel->clearFlags(ChangeFlags::RELATION_ATTEMPTED |
        ChangeFlags::RELATION_DEFERRED);
    if (rel->hasActualChanges())
    {
        // There may be cases where a relation may not
        // actually have changes (e.g. child relation that
        // is upgraded to "changed" to force processing of
        // any potential changed grandchild relations);
        // don't push to tile(s) in that case

        assignToTiles(rel);
    }
    return 1;
}


void ChangeManager::assignToTiles(ChangedFeature2D* feature)
{
    if(feature->ref().tip().isNull())
    {
        LOGS << feature->typedId() << " has unresolved refs: "
            << feature->ref() << " / " << feature->refSE();
    }
    assert(!feature->ref().tip().isNull());
    if (feature->ref().tip() == feature->refSE().tip())
    {
        LOGS << feature->typedId() << ": Equal refs = "
            << feature->ref() << " = " << feature->refSE();
    }
    assert(feature->ref().tip() != feature->refSE().tip());

    CRef ref = feature->refSE();
    Tip tip = ref.tip();
    if(!tip.isNull())   [[unlikely]]
    {
        model_.getChangedTile(tip)->addChanged(model_.copy(feature));
        if (feature->id() == 89253924)
        {
            LOGS << "Assigned copy of " << feature->typedId() << " to " << tip;
        }
    }
    ref = feature->ref();
    tip = ref.tip();
    if(tip.isNull())
    {
        LOGS << feature->typedId() << " has null NW ref: " <<
            feature->ref() << " / " << feature->refSE();
    }
    assert(!tip.isNull());
    model_.getChangedTile(tip)->addChanged(feature);
    if (feature->id() == 89253924)
    {
        LOGS << "Assigned " << feature->typedId() << " to " << tip;
    }
}


// TODO: reltables of members need to be updated if parent moved tiles
//  (i.e. flag RELTABLE_LOADED & RELTABLE_CHANGED)
//  No, update only needs to happen if rel changes zoom levels
void ChangeManager::checkMemberExports(ChangedFeature2D* rel)
{
    Tip relTip = rel->ref().tip();
    int relZoom = tileCatalog_.tileOfTip(relTip).zoom();
    bool dualTileRelation = rel->refSE() != CRef::SINGLE_TILE;
    /*
    if(dualTileRelation && rel->refSE().tip().isNull())
    {
        LOGS << "Invalid refs for " << rel->typedId() << ": "
            << rel->ref() << " / " << rel->refSE();
    }
    */
    assert(!dualTileRelation || !rel->refSE().tip().isNull());
    bool relationWillBeForeign = false;
    for (CFeatureStub* memberStub : rel->members())
    {
        if (!memberStub) [[unlikely]]
        {
            continue;   // skip omitted member
        }
        CFeature* member = memberStub->get();
        bool memberWillBeForeign = member->isFutureForeign();
        if (!memberWillBeForeign)
        {
            // Member has not been marked as foreign yet

            memberWillBeForeign = member->ref().tip() != relTip;
            if (dualTileRelation && member->type() != FeatureType::NODE)
            {
                memberWillBeForeign |= member->refSE() != CRef::SINGLE_TILE;
            }
            checkExport(member, memberWillBeForeign);
        }
        relationWillBeForeign |= relZoom !=
            tileCatalog_.tileOfTip(member->ref().tip()).zoom();
    }

    // Relation only needs to be exported if it is at a level
    // different from its members
    // (If it is a foreign member of another relation, its TEX
    //  will be checked by that relation)

    if (!rel->isFutureForeign())
    {
        checkExport(rel, relationWillBeForeign);
    }
}

// TODO: move to ChangeModel
void ChangeManager::checkExport(CFeature* feature, bool willBeForeign)
{
    if(willBeForeign)
    {
        feature->markAsFutureForeign();
        if (!feature->ref().isExported())
        {
            // Member is not_exported or maybe_exported
            model_.mayGainTex(feature);
        }
    }
    else if (feature->ref().mayHaveTex())
    {
        // If the feature may have a TEX and it is local,
        // and either the feature or its parent moved tiles,
        // that means it may lose its TEX
        // In a later step, we'll check if it is foreign in any
        // other way or relation

        // TODO: This does not work, we need to check SE ref as well
        //  Could be new to NW because of bbox expansion, but still
        //  remain in its original SE tile (or the former NW tile
        //  is now its SE tile)

        // TODO: WE also need to consider *unresolved* refs;
        //  these could also have a TEX that may need to be dropped

        // TODO: use a flag so we can use a vector instead of hashset
        model_.mayLoseTex(feature);
    }
}


const CTagTable* ChangeManager::getExceptionNodeTags(bool duplicate, bool orphan)
{
    assert(duplicate || orphan);
    const CTagTable** pTable = orphan ?
        (duplicate ? &duplicateOrphanNodeTags_ : &orphanNodeTags_) :
            &duplicateNodeTags_;
    if (*pTable == nullptr)
    {
        *pTable = model_.createExceptionNodeTags(duplicate, orphan);
    }
    return *pTable;
}

// We need the TIP where this coordinate is located, so we can
//  build a ref for the remaining unique node, in case we have to
//  add it to the change model
ChangedNode* ChangeManager::findUniqueLocationNode(Tip tip, Coordinate xy)
{
    auto it = uniqueLocationNodes_.find(xy);
    if (it != uniqueLocationNodes_.end()) return it->second;
    ChangedNode* node = model_.nodeAtFutureLocation(xy);
    if (node && node->is(ChangeFlags::FLAGGED_SHARED_LOCATION))
    {
        uniqueLocationNodes_[xy] = nullptr;
        return nullptr;
    }

    NodePtr  soleRemainingNode;
    Query query(model_.store(), Box(xy), FeatureTypes::NODES);
    for (;;)
    {
        FeaturePtr otherNode = query.next();
        if (otherNode.isNull()) break;
        assert(otherNode.isNode());
        CFeature* f = model_.peekFeature(TypedFeatureId::ofNode(otherNode.id()));
        if (f && f->isChanged())
        {
            ChangedNode* changed = ChangedNode::cast(f);
            if (changed->is(ChangeFlags::GEOMETRY_CHANGED))
            {
                continue;
            }
        }
        if (!soleRemainingNode.isNull())
        {
            // There are more than one node remaining at this location
            uniqueLocationNodes_[xy] = nullptr;
            return nullptr;
        }
        soleRemainingNode = NodePtr(otherNode);
    }
    node = model_.getChangedNode(soleRemainingNode.id());
    node->setXY(xy);
    TilePtr tile = model_.store()->fetchTile(tip);
    node->offerRef(CRef::ofMaybeExported(
        tip, tile.handleOf(soleRemainingNode)));
    uniqueLocationNodes_[xy] = node;
    return node;
}


CRef ChangeManager::getRef(FeaturePtr feature) const
{
    Tip tip;
    if (feature.isNode()) [[unlikely]]
    {
        tip = tileCatalog_.tipOfCoordinateSlow(NodePtr(feature).xy());
    }
    else
    {
        TilePair tp = tileCatalog_.tilePair(feature.bounds());
        tip = tileCatalog_.tipOfTile(
            tp[feature.hasNorthwestTwin() ? 1 : 0]);
    }
    return CRef::ofMaybeExported(tip,
        store()->fetchTile(tip).handleOf(feature));
}


ChangedTile* ChangeManager::getChangedTile(Tip tip)
{
    assert(!tip.isNull());
    auto it = changedTiles_.find(tip);
    if(it != changedTiles_.end()) return it->second;
    Arena& arena = model_.arena();
    ChangedTile* changedTile = arena.create<ChangedTile>(arena, tip);
    changedTiles_[tip] = changedTile;
    return changedTile;
}

// TODO: possible replacement for checkExport()
/*
void Updater::mayGainOrLoseTex(CFeature* member, ChangedFeature2D* parent)
{
    assert((member->isChanged() &&
        ChangedFeatureBase::cast(member)->is(ChangeFlags::TILES_CHANGED)) ||
        parent->is(ChangeFlags::TILES_CHANGED));

    if(member->isFutureForeign())
    {
        if (!member->ref().isExported())
        {
            // Member is not_exported or maybe_exported
            model_.mayGainTex(member);
        }
    }
    else if (member->ref().mayHaveTex() && parent->is(ChangeFlags::TILES_CHANGED))
    {
        // If the feature may have a TEX and it is local,
        // and either the feature or its parent moved tiles,
        // that means it may lose its TEX
        // In a later step, we'll check if it is foreign in any
        // other way or relation
        model_.mayLoseTex(member);
    }
}
*/


// Rules for orphan nodes:
//   Node becomes an orphan:
//   - If node won't belong to any relation and won't have tags:
//     - ,It is flagged MAY_BECOME_ORPHAN, and
//   - It is NOT flagged ADDED_TO


/// A node becomes an orphan if it
///   1) has no tags,
///   2) does not belong to any way
///   3) does not belong to any relations
///
/// A node turns orphan if it won't have tags, won't be a relation
/// member, and it
/// Has tags               --> not an orphan
/// Is relation member     --> not an orphan
/// Will have waynode flag --> not an orphan
/// Was a feature          --> orphan
/// Marked "dropped from way":
///   still member of any ways  --> not an orphan
/// else:                  --> orphan