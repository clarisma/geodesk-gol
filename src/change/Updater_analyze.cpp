// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "Updater.h"

#include <ranges>
#include <clarisma/zip/Zip.h>
#include <geodesk/feature/FeatureStore.h>
#include <geodesk/feature/ParentRelationIterator.h>
#include <geodesk/query/SimpleQuery.h>
#include <geodesk/query/TileIndexWalker.h>
#include <tile/tes/TesChecker.h>

#include "ChangeReader.h"
#include "change/model/ChangeModelDumper.h"
#include "change/model/ChangedTile.h"
#include "tile/compiler/TileCompiler.h"


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

void Updater::processChanges()
{
    LOGS << "Processing changes...";

    model_.addNewRelationMemberships();
    processNodes();
    processWays();
    processRelations();

    // TODO: Perform secondary search

    // ways deferred due to unknown nodes
    processWays();

    while (!model_.changedRelations().isEmpty())
    {
        processRelations();
    }

    model_.determineTexLosers();

    // TODO: process nodes, ways, relations whose reltables need to be
    //  updated because their parent relations have moved tiles
#ifndef NDEBUG
    ChangeModelDumper dumper(model_);
    dumper.dump("c:\\geodesk\\tests\\dexxu-change-model.txt");
#endif

    LOGS << "Processed changes.";
}


void Updater::processNodes()
{
    LinkedStack nodes(std::move(model_.changedNodes()));
    while(!nodes.isEmpty())
    {
        ChangedNode* node = nodes.pop();
        processNode(node);
    }
}


void Updater::processWays()
{
    LinkedStack ways(std::move(model_.changedWays()));
    while(!ways.isEmpty())
    {
        ChangedFeature2D* way = ways.pop();
        processWay(way);
    }
}



void Updater::processRelations()
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


void Updater::processNode(ChangedNode* node)
{
    if(node->id() == 10711194568)
    {
        LOGS << "Processing node/" << node->id()
            << ", version: " << node->version()
            << ", ref: " << node->ref()
            << ", flags: " << static_cast<uint32_t>(node->flags());
    }
    CRef pastRef = node->ref();
    Tip pastTip = pastRef.tip();
    NodePtr pastNode = node->getFeature(store());

    // TODO: Process case where a feature node is added to a way
    //  for the first time, requiring its waynode_flag to be set
    //  (can be an implicit change without any other changes to the node)
    //  Adding an orphan node to a way revokes its orphan status
    //  and may cause it to become anonymous

    if(node->isDeleted())
    {
        if(!pastTip.isNull())
        {
            model_.getChangedTile(pastTip)->deletedNodes().push(node);
        }
        node->setRef(CRef::MISSING);
        node->addFlags(ChangeFlags::PROCESSED);
        return;
    }

    if (node->xy().isNull())    [[unlikely]]
    {
        node->setRef(CRef::MISSING);
        return;
    }

    processMembershipChanges(node);

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
        willHaveTags = pastNode.isNull() ? false : !pastNode.tags().isEmpty();
    }

    // TODO: determine if dropped from all relations
    bool willBeRelationMember;
    if(testAny(changeFlags, ChangeFlags::ADDED_TO_RELATION |
        ChangeFlags::REMOVED_FROM_RELATION))
    {
        willBeRelationMember = node->parentRelations() != nullptr;
    }
    else
    {
        willBeRelationMember = pastNode.isNull() ? false : pastNode.isRelationMember();
    }

    // TODO: duplicate, orphan

    Tip futureTip = tileCatalog_.tipOfCoordinateSlow(node->xy());
    bool willBeFeature = willHaveTags | willBeRelationMember;
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
            node->addFlags(ChangeFlags::NEW_TO_NORTHWEST | ChangeFlags::TILES_CHANGED);
            // If node moves to another tile, we will need to write its tags
            //  and rels
            if (!node->tagTable())
            {
                const CTagTable* tags = pastRef.tip().isNull() ?
                    &CTagTable::EMPTY : model_.getTagTable(pastRef);
                assert(tags);
                node->setTagTable(tags);
            }
            if (!node->parentRelations())
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
        if (node->is(ChangeFlags::GEOMETRY_CHANGED))
        {
            // If node is (and was) a feature node nad has moved,
            // its parent relations (if any) may implicitly change
            // (If node is added to a relation for the first time,
            // we won't need to call this method, since its parent
            // relations by definition already explicitly change)
            model_.cascadeMemberChange(pastNode, node);
        }
    }
    else
    {
        // TODO: We need to prevent a changed node that is not a feature
        //  from being written into the TES
        //  There is probably a better way to do this
        //  --> If we don't push it to the changedNodes stack,
        //      why would ChangeWriter write it to the TES??
        node->clearFlags(ChangeFlags::TAGS_CHANGED | ChangeFlags::GEOMETRY_CHANGED);
        node->setRef(node->ref() == CRef::MISSING ?
            CRef::MISSING : CRef::ANONYMOUS_NODE);
    }

    // TODO
    node->addFlags(ChangeFlags::PROCESSED);
}

void Updater::addDeleted(Tip tip, ChangedFeatureStub* feature)
{
    assert(feature->type() != FeatureType::NODE);
    ChangedTile* tile = model_.getChangedTile(tip);
    (feature->type() == FeatureType::WAY ?
        tile->deletedWays() : tile->deletedRelations()).push(feature);
}

void Updater::processDeletedFeature(ChangedFeature2D* deleted)
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


void Updater::processMembershipChanges(ChangedFeatureBase* feature)
{
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
void Updater::processWay(ChangedFeature2D* way)
{
    if(way->id() == 393548000 || way->id() == 215088731)
    {
        LOGS << "Processing " << way->typedId();
    }

    if(way->isDeleted())
    {
        processDeletedFeature(way);
        return;
    }

    if (!way->isChangedExplicitly())
    {
        if (normalizeRefs(way) < 1) return;
        // TODO: For both unknown and missing, we need to push the way back
        //  onto the stack of changed ways
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

///
/// @param changed
/// @return  1   if at least one ref has been resolved
///          0   if feature is missing
///         -1   if feature refs are unknown (search required)
///
int Updater::normalizeRefs(ChangedFeature2D* changed)
{
    assert(!changed->isChangedExplicitly());

    // For a feature that has been changed implicitly, we may not
    // have searched any of its tiles, but we must at least have one
    // ref (NW or SE). If the other ref is MISSING, we will set it to
    // either UNRESOLVED (i.e. we know the TIP, but don't have its
    // offset or TEX), or SINGLE_TILE
    CRef ref = changed->ref();
    Tip tip = ref.tip();
    if(!tip.isNull())
    {
        if (changed->refSE().tip().isNull())
        {
            Box pastBounds = ref.getFeature(store()).bounds();
            Box tileBounds = tileCatalog_.tileOfTip(tip).bounds();
            if(pastBounds.maxX() > tileBounds.maxX() ||
                pastBounds.minY() < tileBounds.minY())
            {
                // The feature's bounds extend past the right or
                // bottom edge of its NW tile, which means it has
                // a SE tile
                changed->setRefSE(CRef::ofUnresolved(
                    tileCatalog_.tipOfCoordinateSlow(
                        pastBounds.bottomRight())));
            }
            else
            {
                changed->setRefSE(CRef::SINGLE_TILE);
            }
        }
    }
    else
    {
        ref = changed->refSE();
        tip = ref.tip();
        if (tip.isNull())
        {
            if (memberSearchCompleted_)
            {
                changed->setRef(CRef::MISSING);
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

        changed->setRef(CRef::ofUnresolved(
            tileCatalog_.tipOfCoordinateSlow(
               pastBounds.topLeft())));
    }
    return 1;
}

CRef Updater::deduceTwinRef(CRef ref) const
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


void Updater::updateBounds(ChangedFeature2D* feature, const Box& bounds)
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
        model_.cascadeMemberChange(feature->getFeature(store()), feature);

        // If bounds changed, tiles may change


        TilePair futureTiles(tileCatalog_.tileOfCoordinateSlow(
            bounds.bottomLeft()));
        futureTiles += tileCatalog_.tileOfCoordinateSlow(bounds.topRight());
        // TODO: this is sub-optimal
        futureTiles = tileCatalog_.normalizedTilePair(futureTiles);
        updateTiles(feature, futureTiles);
    }
}


void Updater::updateTiles(ChangedFeature2D* feature, TilePair futureTiles)
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

void Updater::cascadeNodeCoordinateChange(NodePtr node, Coordinate futureXY)
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

void Updater::cascadeBoundsChange(FeaturePtr feature, const Box& futureBounds)
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
int Updater::processRelation(ChangedFeature2D* rel) // NOLINT recursive
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
        if (rel->parentRelations())
        {
            LOGS << rel->typedId() << " has "
                << rel->parentRelations()->relations().size()
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


void Updater::assignToTiles(ChangedFeature2D* feature)
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
void Updater::checkMemberExports(ChangedFeature2D* rel)
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
void Updater::checkExport(CFeature* feature, bool willBeForeign)
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


