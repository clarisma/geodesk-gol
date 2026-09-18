// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangeManager.h"
#include <clarisma/io/FilePath.h>
#include <clarisma/util/Pointers.h>
#include <geodesk/feature/ParentRelationIterator.h>
#include <geodesk/query/ParentWaysQuery.h>
#include "change/model/ChangeModelDumper.h"
#include "change/model/ChangedTile.h"
#include "change/process/NodeProcessor.h"
#include "change/process/WayProcessor.h"
#include "change/process/RelationProcessor.h"
#include "geodesk/query/FeatureFinder.h"

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
    Box maxBounds = Box::ofWorld();
    rel = model_.changedRelations().first();
        // TODO: We need to move the stack locally, because cascading
        //  may add new relations (though that's probably benign)

    // TODO: Is a change in area status of a member considered
    //  a geometric change in the parent relation?

    while (rel)
    {
        ChangeFlags flags = rel->flags();
        if(testAny(flags, ChangeFlags::MEMBERS_CHANGED |
            ChangeFlags::DELETED))
        {
            // If a relation's members have changed, we mark
            //  all of its parents as having changed GEOMETRY
            //  and BOUNDS (this forces processing, which may
            //  clear these flags); if the relation is deleted,
            //  we mark its direct parents MEMBERS_CHANGED, as well
            //  (in case the .osc files don't remove the deleted
            //   relation from its parents)
            ChangeFlags cascadeFlags = ChangeFlags::GEOMETRY_CHANGED |
                ChangeFlags::BOUNDS_CHANGED |
                    (test(flags, ChangeFlags::DELETED) ?
                        ChangeFlags::MEMBERS_CHANGED : ChangeFlags::NONE);
            model_.memberChanged(rel, maxBounds,
                maxBounds, cascadeFlags);
        }
        rel = rel->next();
    }

    // TODO: For explicitly changed relations, we need to cascade
    //  GEOM/BOUNDS changes to their parents; we'll check for refcycles
    //  at the same time
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
        NodeProcessor (*this, *node).process();
    }
    if (!model_.changedNodes().isEmpty())   [[unlikely]]
    {
        // Pick up any coincident nodes that turn into
        //  unique-location nodes
        processNodes();
    }
}


void ChangeManager::processWays()
{
    LinkedStack ways(std::move(model_.changedWays()));
    while(!ways.isEmpty())
    {
        ChangedFeature2D* way = ways.pop();
        WayProcessor (*this, *way).process();
    }
}


void ChangeManager::processRelations()
{
    LinkedStack relations(std::move(model_.changedRelations()));
    while(!relations.isEmpty())
    {
        ChangedFeature2D* rel = relations.pop();
        RelationProcessor (*this, *rel).process();
    }
}


// TODO: move to ChangeModel -- no, relies on TileCatalog
void ChangeManager::wayNodeFeatureStatusChanged(Coordinate xy, NodePtr node)
{
    ParentWaysQuery query(store(), xy, node);
    for (;;)
    {
        WayPtr way = query.next();
        if (way.isNull()) break;
        if (way.id() == 1154460013)
        {
            LOGS << "!!!";
        }
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
        model_.ensureBounds(changedWay);
        changedWay->addFlags(ChangeFlags::MEMBERS_CHANGED);
    }
}


/*


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
        // model_.memberGeometryChanged(way);
        // TODO: cascade changes to parent relations
        //  but should be called later, needs to report
        //  geom/bounds changes as well as deletion (member change in parent)
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

*/


/// Past bounds must be set
///
/// @param feature
/// @return  1   if at least one ref has been resolved
///          0   if feature is missing
///         -1   if feature refs are unknown (search required)
///
// TODO: This may be wrong, because it looks up the tile of
//  the topLeft/bottomRight coordinate; in reality, the
//  true twin-tile may be at a lower zoom level, we need to
//  look at the bounds of the feature to determine its level
int ChangeManager::normalizeRefs(CFeature* feature) const
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
                assert(tileCatalog_.tipOfTile(tp.first()) == tip);
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
        assert(tileCatalog_.tipOfTile(tp.second()) == tip);
        assert(tp.hasSecond());
        ref = CRef::ofUnresolved(tileCatalog_.tipOfTile(tp.first()));
        feature->setRef(ref);
    }
    return 1;
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
    if (!soleRemainingNode.isNull())
    {
        node = model_.getChangedNode(soleRemainingNode.id());
        node->setXY(xy);
        TilePtr tile = model_.store()->fetchTile(tip);
        node->offerRef(CRef::ofMaybeExported(
            tip, tile.handleOf(soleRemainingNode)));
        uniqueLocationNodes_[xy] = node;
    }
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
    ChangedTile* changedTile = arena.create<ChangedTile>(tip,
        tileCatalog_.tileOfTip(tip), store()->fetchTile(tip));
    changedTiles_[tip] = changedTile;
    return changedTile;
}

void ChangeManager::remove(ChangedFeatureBase* feature, bool fromSE, bool useOriginal)
{
    CRef ref = feature->ref(fromSE);
    Tip tip = ref.tip();
    if (tip.isNull())
    {
        LOGS << "Attempt to remove missing " << feature->typedId();
    }
    assert(!tip.isNull());
    ChangedTile* tile = getChangedTile(tip);

    // TODO: Always make a copy so we don't need this check?
    ChangedFeatureStub* maybeCopy = feature;
    if (!useOriginal)
    {
        maybeCopy = model_.copy(feature);
    }

    tile->deletedFeatures(feature->type()).push(maybeCopy);
    if (ref.mayHaveTex())
    {
        texChange(feature, fromSE, false);
    }
}

void ChangeManager::texChange(CFeature* feature, bool inSE, bool texNeeded)
{
    assert(feature->type() != FeatureType::NODE || !inSE);
        // Nodes are single-tile and hence can only be in a NW tile

    int32_t handle;
    CRef ref = feature->ref(inSE);
    Tip tip = ref.tip();
    assert(!tip.isNull());
    ChangedTile* changedTile = getChangedTile(tip);
    if (ref.isNew())
    {
        assert(texNeeded);
            // For a new ref, requesting a new TEX is the only
            // valid TEX change; a new ref cannot have a TEX
            // that needs dropping
        handle = 0;
    }
    else
    {
        TilePtr pTile = store()->fetchTile(tip);
        assert(pTile);
        // TODO: What happens if the tile is not loaded?
        FeaturePtr fp = ref.getFeature(pTile);
        if (fp.isNull())    [[unlikely]]
        {
            assert (feature->type() != FeatureType::NODE);
            // We cannot perform alt-tile resolution
            // for nodes, because ndoes are single-tile
            CRef otherRef = feature->ref(!inSE);
            Tip otherTip = otherRef.tip();
            assert(!otherTip.isNull());
            TilePtr pTileOther = store()->fetchTile(otherTip);
            // TODO: what happens if the other tile is not loaded?
            assert(pTileOther);
            // Retrieve the feature from the other tile
            fp = otherRef.getFeature(pTileOther);
            assert(!fp.isNull());
            Box bounds = fp.bounds();
            Coordinate corner = inSE ? bounds.bottomRight() : bounds.topLeft();
            bounds = corner;
            FeatureFinder finder;
            // Now look up the feature in the original tile
            fp = finder.find(pTile, feature->typedId(), bounds);
            assert(!fp.isNull());
        }
        handle = pTile.handleOf(fp);
    }
    changedTile->texChange(handle, texNeeded ? feature : nullptr);
    if (!texNeeded)
    {
        ref = CRef::ofNotExported(tip, handle);
        if (inSE)
        {
            feature->setRefSE(ref);
        }
        else
        {
            feature->setRef(ref);
        }
    }
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
///
///

void ChangeManager::confirmTexLoss(ChangedFeatureBase* feature)
{
    if (feature->isFutureForeign())
    {
        // Feature will be foreign -> it definitely keeps its TEX
        return;
    }
    CRef ref = feature->ref();
    Tip tip = ref.tip();
    assert(!tip.isNull());
    if (!ref.mayHaveTex())
    {
        // Feature already lost its TEX
        // TODO: How about unresolved?
        return;
    }

    CRef refSE = (feature->type() == FeatureType::NODE) ?
        CRef::SINGLE_TILE : feature->refSE();

    const CRelationTable* rels = model_.getParentRelations(feature);
    if (rels)
    {
        for (CFeatureStub* relStub : rels->relations())
        {
            CFeature* rel = relStub->get();
            if (tip != rel->ref().tip())
            {
                // Parent relation is in different tile
                // -> foreign, keep TEX
                feature->markAsFutureForeign();
                return;
            }
            if (refSE.tip() != rel->refSE().tip())
            {
                // SE tiles differ
                // -> foreign, keep TEX
                feature->markAsFutureForeign();
                return;
            }
        }
    }
    if (feature->type() == FeatureType::NODE)
    {
        if (!feature->is(ChangeFlags::TILES_CHANGED))
        {
            // The node has not changed tiles (changing tiles would have
            // triggered all of its parent ways to re-validate foreign
            // relationships, so it would have been marked as foreign
            // already if its TEX were still needed.
            // If a node has not moved tiles, we can still determine
            // its past position via its ref

            TilePtr pNodeTile = store()->fetchTile(tip);
            Box nodeTileBounds = tileCatalog().tileOfTip(tip).bounds();
            FeaturePtr pastFeature = ref.getFeature(pNodeTile);
            assert(!pastFeature.isNull());
            NodePtr pastNode(pastFeature);
            ParentWaysQuery query(store(), pastNode.xy(), pastNode);
            for (;;)
            {
                WayPtr way = query.next();
                if (way.isNull()) break;
                if (!pNodeTile.contains(way))
                {
                    // Parent way is in a different tile
                    // -> node must be foreign
                    feature->markAsFutureForeign();
                    return;
                }
                const Box& wayBounds = way.bounds();
                if (wayBounds.maxX() > nodeTileBounds.maxX() ||
                    wayBounds.maxY() > nodeTileBounds.maxY())
                {
                    // Parent way is dual-tile
                    // -> node must be foreign
                    feature->markAsFutureForeign();
                    return;
                }
            }
        }
    }

    // The feature is no longer foreign, so its TEX can be released

    texChange(feature, false, false);
    if (refSE != CRef::SINGLE_TILE)
    {
        texChange(feature, true, false);
    }
    // texChange() already converts the refs into non-exported refs
}


// TODO
void ChangeManager::ensureResolved(const CRelationTable* rels)
{
    for (CFeatureStub* relStub : rels->relations())
    {
        CFeature* rel = relStub->get();
        int result = normalizeRefs(rel);
        assert(result > 0);

    }
}
