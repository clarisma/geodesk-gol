// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "FeatureProcessor.h"
#include "geodesk/geom/FixedLonLat.h"

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
	using enum ChangeFlags;

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
		if (node().id() == 2485099190)
		{
			LOGS << node().typedId();
		}
		processMembershipChanges();
		if(is(DELETED))
		{
			// TODO: Deleting a missing node is a no-op
			if(!pastTip_.isNull())
			{
				model().getParentRelations(&node());
				// We need to ensure that the reltable is
				// loaded for a feature that has been deleted
				// without being removed from its parent relations,
				// so we can cascade
				// (Once we clear refs, we can no longer fetch
				// the original table)

				if (pastFeatureFlags_ & FeatureFlags::SHARED_LOCATION)
				{
					mgr_.findUniqueLocationNode(pastTip_, pastXY_);
				}

				remove(false, true);
			}
			node().setRef(CRef::MISSING);

			// If node was a feature, a delete has to be modify
			// any parent ways & relations via cascade, because we
			// cannot be guaranteed that the node has been removed
			// from those parents (we cannot assume that the osc
			// respects referential integrity)
			// That's why we fall through to the end
		}
		else
		{
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
		}
		propagateFeatureStatusChangeToWays();
		if (isAny(DELETED | GEOMETRY_CHANGED))
		{
			// TODO: We can probably make this more efficient,
			//  need to do this only if node has been deleted
			//  or will be a relation member

			// If node is (and was) a feature node and has moved,
			// its parent relations (if any) may implicitly change
			// (If node is added to a relation for the first time,
			// we won't need to call this method, since its parent
			// relations by definition already explicitly change)
			// model_.cascadeMemberChange(pastNode, node);

			if (node().id() == 7898016044)
			{
				LOGS << "Cascading geometry change of node/" << node().id();
			}
			Box pastBounds = pastXY_;
			Box futureBounds = node().xy();
			model().memberChanged(&node(), pastBounds, futureBounds,
				GEOMETRY_CHANGED | BOUNDS_CHANGED |
					(isAny(DELETED | TILES_CHANGED) ?
						MEMBERS_CHANGED : NONE));

			// If a node's geometry changes, its bounds are always changed
			// (This is important because parent relations only consider
			// bounds change of a member when determining whether their
			// own bounds could have changed)
		}
		addFlags(PROCESSED);
		tallyStats();

		if (node().id() == 10816096480)
		{
			LOGS << node().typedId() << ": ref after processing is " << node().ref();
		}

		// TODO: If node changes tiles and is exported, it must notify
		//  its parent ways so the node table can be updated
		//  (or do we do this already whenever geom is changed?)
	}

private:
	void resolveTags()
	{
		if (is(TAGS_CHANGED))
		{
			assert(node().tagTable());
			willHaveTags_ = node().tagTable() != &CTagTable::EMPTY;
			/*
			if (pastFeatureFlags_ & FeatureFlags::EXCEPTION_NODE)  [[unlikely]]
			{
				// If the node was an orphan or duplicate, empty tags
				// in the change instructions means no tags change
				// TODO: The TCA should have identified this already
				//  and cleared TAGS_CHANGED
				if (!willHaveTags_)
				{
					clearFlags(TAGS_CHANGED);
				}
			}
			*/
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
		if(isAny(ADDED_TO_RELATION | REMOVED_FROM_RELATION))
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
	        if (is(REMOVED_FROM_WAY))
	        {
	        	if (!is(GEOMETRY_CHANGED))
	        	{
	        		// If the node has been removed from a way, we now need
	        		// to check if it still belongs to at least one way
	        		// We assume the answer is "no"

	        		if (node().id() == 21432484)
	        		{
	        			LOGS << node().typedId() << " was at "
							<< FixedLonLat(pastXY_) << ", now at "
							<< FixedLonLat(node().xy());
	        		}

	        		// assert(!is(GEOMETRY_CHANGED));

	        		// This does not work for anon nodes that have been
	        		// moved, because we don't have a way to store their
	        		// past location (which we need for a parent search)
	        		// But a moved anon node will cause all its past parent
	        		// ways to become implicitly changed, so we have to
	        		// already mark it as future waynode when we discover
	        		// that implicit way change

	        		ParentWaysQuery query(mgr_.store(), pastXY_, pastNode_);
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
	        }
	        else
	        {
	            willBelongToWay_ = hasBelongedToWay_;
	        }
	    }

		ChangeFlags flagsToAdd = willBelongToWay_ ?
			FLAGGED_WAYNODE : NONE;
	    flagsToAdd |= (hasBelongedToWay_ != willBelongToWay_) ?
	        FLAGS_CHANGED : NONE;
		addFlags(flagsToAdd);
	}

	void resolveCoincidentLocation()
	{
	    wasCoincident_ = pastFeatureFlags_ & FeatureFlags::SHARED_LOCATION;
		willBeCoincident_ = is(FLAGGED_SHARED_LOCATION);

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
	            if (is(GEOMETRY_CHANGED))
	            {
	                // If the formerly coincident node moved, and it is
	                // not coincident at its new location, it loses its
	                // SHARED_LOCATION flag (already cleared, but we
	                // need to mark the flag change so the node will be
	                // updated)
	                addFlags(FLAGS_CHANGED);
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
			FLAGS_CHANGED : NONE);
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
			FLAGGED_EXCEPTION_NODE : NONE);

		if (wasDuplicate_ != willBeDuplicate_ || wasOrphan_ != willBeOrphan_) [[unlikely]]
		{
			addFlags(FLAGS_CHANGED);
			if (willBeOrphan_ || willBeDuplicate_)
			{
				node().setTagTable(mgr_.getExceptionNodeTags(
					willBeDuplicate_, willBeOrphan_));
				addFlags(TAGS_CHANGED);
			}
			else
			{
				if (!willHaveTags_)
				{
					node().setTagTable(&CTagTable::EMPTY);
					addFlags(TAGS_CHANGED);
				}
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
	        	ensureReltableLoaded();
	        	remove(false);
	        }
	        if(!futureTip.isNull())
	        {
	            node().setRef(CRef::ofNew(futureTip));
	            addFlags(NEW_TO_NORTHWEST | TILES_CHANGED);
	            // If node moves to another tile, we will need to write its tags
	            //  and rels
	            if (!node().tagTable())
	            {
	                const CTagTable* tags = pastRef_.tip().isNull() ?
	                    &CTagTable::EMPTY : model().getTagTable(pastRef_);
	                assert(tags);
	                node().setTagTable(tags);
	            }
	            if (!is(RELTABLE_LOADED))
	            {
	                node().setParentRelations(model().getRelationTable(pastRef_));
	            }
	        }
	        else
	        {
	        	// TODO: Check this
	        	/*
	            if(node().isFutureWaynode())
	            {
		            node().setRef(CRef::ANONYMOUS_NODE);
	            }
	            */
	        	node().setRef(CRef::ANONYMOUS_NODE);
	        }
	    }
	    if(!futureTip.isNull())
	    {
	    	if (node().hasActualChanges()) [[likely]]
	    	{
	    		// Only push node to tiles if it has actual changes
	    		ChangedTile* futureTile = mgr_.getChangedTile(futureTip);
	    		futureTile->changedNodes().push(&node());
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
	        clearFlags(TAGS_CHANGED | GEOMETRY_CHANGED);
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

			// if (!is(ChangeFlags::GEOMETRY_CHANGED))
			// {

			// Only do this if the node hasn't moved (for nodes that
			// moved, the TileChangeAnalyzer has already marked their
			// implicitly changed parent ways
			// No! We need to mark parents as members_changed
			// TODO: didn't fix the 10/3/26 problem

			if (willBeFeature_ || (pastFeatureFlags_ & FeatureFlags::WAYNODE) != 0)
			{
				// Only do this if an anonymous node (which is always a waynode)
				// turn feature node, or a waynode-flagged feature node turns
				// anonymous

				mgr_.wayNodeFeatureStatusChanged(pastXY_, pastNode_);
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
