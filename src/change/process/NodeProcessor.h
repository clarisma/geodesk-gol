// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "FeatureProcessor.h"

// TODO: A feature staus change is a tile change, makes
//  cascading logic easier: cascade geometry, tiles
// TODO: But don't cascade new nodes; cascades are only
//  for existing nodes

// old todos, review:

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

// TODO: getParentRelations() can mutate flags, which
//  we then clobber when we set changeFlags!!!

class NodeProcessor : public FeatureProcessor
{
public:
	NodeProcessor(ChangeManager& mgr, ChangedNode& node) :
		FeatureProcessor(mgr, node),
		pastRef_(node.ref()),
		pastTip_(pastRef_.tip()),
		pastNode_(node.getFeature(mgr.store())),
		pastXY_(node.xy())
	{
		if (!pastNode_.isNull())
		{
			pastFeatureFlags_ = pastNode_.flags();
			pastXY_ = pastNode_.xy();
		}
		// TODO: What is the pastXY for a new node?
		//  New nodes have GEOMETRY_CHANGED set,
		//  which may cascade to parent rels, so we
		//  would need pastXY
		//  safest to default past XY to future XY
	}

	void process()
	{
		processMembershipChanges();
		if(is(ChangeFlags::DELETED))
		{
			// TODO: Deleting a missing node is a no-op
			if(!pastTip_.isNull())
			{
				remove(false);
			}
			node().setRef(CRef::MISSING);
			addFlags(ChangeFlags::PROCESSED);
			return;

			// TODO: If node was a feature, a delete has to be modify
			//  any parent ways & relations via cascade, because we
			//  cannot be guaranteed that the node has been removed
			//  from those parents (we cannot assume that the osc
			//  respects referential integrity)
		}

		if (node().xy().isNull())    [[unlikely]]
		{
			// TODO: Can we just avoid this scenario that CRef is
			//  set but x/y is not, so we don't have to fix it here?

			if (!pastNode_.isNull())
			{
				node().setXY(pastNode_.xy());
			}
			if (node().xy().isNull())
			{
				node().setRef(CRef::MISSING);
				return;
			}
			// TODO: still need to propagate
		}

		resolveTags();
		resolveMemberStatus();
		resolveWaynodeStatus();
		resolveCoincidentLocation();
		resolveExceptionStatus();
		resolveFeatureStatus();
		resolveTileChange();
		propagateFeatureStatusChangeToWays();
		addFlags(ChangeFlags::PROCESSED);

		// TODO: If node changes tiles and is exported, it must notify
		//  its parent ways so the node table can be updated
		//  (or do we do this already whenever geom is changed?)
	}

private:
	void resolveTags()
	{
		if (is(ChangeFlags::TAGS_CHANGED))
		{
			assert(node().tagTable());
			willHaveTags_ = node().tagTable() != &CTagTable::EMPTY;
		}
		else
		{
			if (!pastNode_.isNull())
			{
				willHaveTags_ = !pastNode_.tags().isEmpty() &&
					(pastFeatureFlags_ & FeatureFlags::EXCEPTION_NODE) == 0;
				// An exception node (orphan or duplicate) has synthetic tags;
				// these don't count as "having tags"
			}
		}
	}

	void resolveMemberStatus()
	{
		if(isAny(ChangeFlags::ADDED_TO_RELATION |
			ChangeFlags::REMOVED_FROM_RELATION))
		{
			willBeRelationMember_ = node().peekParentRelations() != nullptr;
		}
		else if (!pastNode_.isNull())
		{
			willBeRelationMember_ = pastNode_.isRelationMember();
		}
	}

	void resolveWaynodeStatus()
	{
		hasBelongedToWay_ = pastRef_ == CRef::ANONYMOUS_NODE ||
			(pastFeatureFlags_ & FeatureFlags::WAYNODE);
		willBelongToWay_ = node().isFutureWaynode();
		if (!willBelongToWay_)
	    {
	        if (is(ChangeFlags::REMOVED_FROM_WAY))
	        {
	            // If the node has been removed from a way, we now need
	            // to check if it still belongs to at least one way
	            // We assume the answer is "no"

	        	ParentWaysQuery query(mgr_.store(), node().xy(), pastNode_);
	            for (;;)
	            {
	                WayPtr way = query.next();
	                if (way.isNull()) break;
	                CFeature* feature = model().peekFeature(
	                	TypedFeatureId::ofWay(way.id()));
	                if (feature == nullptr || !feature->isChanged())
	                {
	                    // If the anon node belonged to a way that is not
	                    // tracked by the model or hasn't changed, we know
	                    // it still belongs to that way
	                    willBelongToWay_ = true;
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
	                    willBelongToWay_ = true;
	                    break;
	                }
	            }
	        }
	        else
	        {
	            willBelongToWay_ = hasBelongedToWay_;
	        }
	    }

		ChangeFlags flagsToAdd = willBelongToWay_ ?
			ChangeFlags::FLAGGED_WAYNODE : ChangeFlags::NONE;
	    flagsToAdd |= (hasBelongedToWay_ != willBelongToWay_) ?
	        ChangeFlags::FLAGS_CHANGED : ChangeFlags::NONE;
		addFlags(flagsToAdd);
	}

	void resolveCoincidentLocation()
	{
	    wasCoincident_ = pastFeatureFlags_ & FeatureFlags::SHARED_LOCATION;
		willBeCoincident_ = is(ChangeFlags::FLAGGED_SHARED_LOCATION);

	    if (wasCoincident_) [[unlikely]]
	    {
	        // If a coincident node moved, we need to check if only
	        // one node remains at its past location -- if so, that
	        // node loses its SHARED_LOCATION flag (and may lose its
	        // feature status if it is untagged, does not belong to
	        // a relation, and is not an orphan).

	        // If a coincident node has not moved, we need to still
	        // check if all other nodes have moved from its location,
	        // causing it to be the sole node that location

	        assert(!pastTip_.isNull());
	        ChangedNode* uniqueNode = mgr_.findUniqueLocationNode(
	        	pastTip_, pastNode_.xy());
	        if (!willBeCoincident_)
	        {
	            if (is(ChangeFlags::GEOMETRY_CHANGED))
	            {
	                // If the formerly coincident node moved, and it is
	                // not coincident at its new location, it loses its
	                // SHARED_LOCATION flag (already cleared, but we
	                // need to mark the flag change so the node will be
	                // updated)
	                addFlags(ChangeFlags::FLAGS_CHANGED);
	            }
	            else
	            {
	                // If the node is not explicitly marked as being coincident
	                // in the future, it will stay coincident if it is not
	                // the unique node at its location
	                if (uniqueNode != &node())
	                {
	                    willBeCoincident_ = true;
	                }
	                else
	                {
	                    // SHARED_LOCATION already cleared,
	                    // the flag change will be marked below
	                }
	            }
	        }
	    }
		addFlags(wasCoincident_ != willBeCoincident_ ?
			ChangeFlags::FLAGS_CHANGED : ChangeFlags::NONE);
	}

	/// Needs:
	/// - coincident location resolved
	/// - tags resolved
	/// - relation member status resolved
	/// - waynode status resolved
	///
	void resolveExceptionStatus()
	{
		wasDuplicate_ = (pastFeatureFlags_ &
			(FeatureFlags::SHARED_LOCATION | FeatureFlags::EXCEPTION_NODE)) ==
			(FeatureFlags::SHARED_LOCATION | FeatureFlags::EXCEPTION_NODE);
		willBeDuplicate_ = willBeCoincident_ && !willHaveTags_;

		// Determine orphan status

		wasOrphan_ = (pastFeatureFlags_ & (FeatureFlags::EXCEPTION_NODE |
			FeatureFlags::WAYNODE | FeatureFlags::RELATION_MEMBER)) == FeatureFlags::EXCEPTION_NODE;
		willBeOrphan_ = !willHaveTags_ && !willBeRelationMember_ && !willBelongToWay_;

		addFlags(willBeDuplicate_ || willBeOrphan_ ?
			ChangeFlags::FLAGGED_EXCEPTION_NODE : ChangeFlags::NONE);

		if (wasDuplicate_ != willBeDuplicate_ || wasOrphan_ != willBeOrphan_) [[unlikely]]
		{
			addFlags(ChangeFlags::FLAGS_CHANGED);
			if (willBeOrphan_ || willBeDuplicate_)
			{
				node().setTagTable(mgr_.getExceptionNodeTags(
					willBeDuplicate_, willBeOrphan_));
				addFlags(ChangeFlags::TAGS_CHANGED);
			}
		}
	}

	void resolveFeatureStatus()
	{
		willBeFeature_ = willHaveTags_ | willBeRelationMember_ |
			willBeOrphan_ | willBeDuplicate_;
	}

	/// Needs:
	/// - willBeFeature_ resolved
	void resolveTileChange()
	{
	    Tip futureTip = mgr_.tileCatalog_.tipOfCoordinateSlow(node().xy());
	    futureTip = willBeFeature_ ? futureTip : Tip();

		// TODO: Check this, we need the future TIP for indexing


	    if(futureTip != pastTip_)
	    {
	        if(!pastTip_.isNull())
	        {
	        	remove(false);
	        }
	        if(!futureTip.isNull())
	        {
	            node().setRef(CRef::ofNew(futureTip));
	            addFlags(ChangeFlags::NEW_TO_NORTHWEST | ChangeFlags::TILES_CHANGED);
	            // If node moves to another tile, we will need to write its tags
	            //  and rels
	            if (!node().tagTable())
	            {
	                const CTagTable* tags = pastRef_.tip().isNull() ?
	                    &CTagTable::EMPTY : model().getTagTable(pastRef_);
	                assert(tags);
	                node().setTagTable(tags);
	            }
	            if (!node().peekParentRelations())
	            {
	                node().setParentRelations(model().getRelationTable(pastRef_));
	            }
	        }
	        else
	        {
	            if(node().isFutureWaynode())
	            {
	                node().setRef(CRef::ANONYMOUS_NODE);
	            }
	        }
	    }
	    if(!futureTip.isNull())
	    {
	        ChangedTile* futureTile = mgr_.getChangedTile(futureTip);
	        futureTile->changedNodes().push(&node());
	        if (is(ChangeFlags::GEOMETRY_CHANGED))
	        {
	            // If node is (and was) a feature node and has moved,
	            // its parent relations (if any) may implicitly change
	            // (If node is added to a relation for the first time,
	            // we won't need to call this method, since its parent
	            // relations by definition already explicitly change)
	            // model_.cascadeMemberChange(pastNode, node);

	            Box pastBounds = pastXY_;
	            Box futureBounds = node().xy();
	            model().memberChanged(&node(), pastBounds, futureBounds,
	                ChangeFlags::GEOMETRY_CHANGED |
	                    (is(ChangeFlags::DELETED) ?
	                        ChangeFlags::MEMBERS_CHANGED : ChangeFlags::NONE));

	            // TODO: This is in the wrong place
	            // TODO: move down, must also call if deleted
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
	        clearFlags(ChangeFlags::TAGS_CHANGED | ChangeFlags::GEOMETRY_CHANGED);
	        setRef(node().ref() == CRef::MISSING ?
	            CRef::MISSING : CRef::ANONYMOUS_NODE);
	    }
	}

	void propagateFeatureStatusChangeToWays()
	{
		bool wasFeature = !pastNode_.isNull();
		if (willBeFeature_ != wasFeature)    [[unlikely]]
		{
			// If a node's feature status has changed, all ways that
			// contain this node need to update their node tables

			if (!is(ChangeFlags::GEOMETRY_CHANGED))
			{
				// Only do this if the node hasn't moved (for nodes that
				// moved, the TileChangeAnalyzer has already marked their
				// implicitly changed parent ways

				if (willBeFeature_ || (pastFeatureFlags_ & FeatureFlags::WAYNODE) != 0)
				{
					// Only do this if an anonymous node (which is always a waynode)
					// turn feature node, or a waynode-flagged feature node turns
					// anonymous

					mgr_.wayNodeFeatureStatusChanged(node().xy(), pastNode_);
				}
			}
		}
	}

	ChangedNode& node() const
	{
		return static_cast<ChangedNode&>(feature_);
		// NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		// cast is safe
	}

	CRef pastRef_;
	Tip pastTip_;
	uint32_t pastFeatureFlags_ = 0;
	NodePtr pastNode_;
	Coordinate pastXY_;
	bool willHaveTags_ = false;
	bool willBeRelationMember_ = false;
	bool hasBelongedToWay_ = false;
	bool willBelongToWay_ = false;
	bool wasCoincident_ = false;
	bool willBeCoincident_ = false;
	bool wasDuplicate_ = false;
	bool willBeDuplicate_ = false;
	bool wasOrphan_ = false;
	bool willBeOrphan_ = false;
	bool willBeFeature_ = false;
};
