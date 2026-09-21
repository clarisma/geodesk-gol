// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "Feature2dProcessor.h"

class WayProcessor : public Feature2dProcessor
{
public:
	WayProcessor(ChangeManager& mgr, ChangedFeature2D& way) :
		Feature2dProcessor(mgr, way)
	{
		assert(way.type() == FeatureType::WAY);
	}

	// TODO: Do we need to deal with WAY_WILL_HAVE_FEATURE_NODES?
	//  Collapse into FLAGGED_WAYNODE?
	void process()
	{
		if (way().id() == 1340662848)
		{
			LOGS << "!!!";
		}
		processMembershipChanges();
		if (normalizeRefs() <= 0) [[unlikely]]
		{
			if (!way().isChangedExplicitly())
			{
				// Missing feature
				addFlags(ChangeFlags::PROCESSED);
				return;
			}
		}
		/*
		if (getRef() != CRef::MISSING && pastBounds_.isEmpty())
		{
			LOGS << "Bounds not set for way/" << way().id();
		}
		assert(getRef() == CRef::MISSING || !pastBounds_.isEmpty());
		*/
		// It's ok for pastBounds to be uninitialized
		//  in cases such as membership change

		if (is(ChangeFlags::DELETED))	[[unlikely]]
		{
			processDeleted();
		}
		else
		{
			if (isAny(ChangeFlags::GEOMETRY_CHANGED |
				ChangeFlags::WAYNODE_IDS_CHANGED | ChangeFlags::MEMBERS_CHANGED))
			{
				ensureNodesLoaded();
				if (!computeBounds()) [[unlikely]]
				{
					// Need to defer because we need to search
					// for nodes whose location is unknown
					model().changedWays().push(&way());
					return;
				}
				if (missingNodes_)	[[unlikely]]
				{
					if (missingNodes_ == way().memberCount())
					{
						// All nodes are missing => delete the way
						addFlags(ChangeFlags::DELETED);
						clearFlags(ChangeFlags::MEMBERS_CHANGED);
						processDeleted();
						futureBounds_ = pastBounds_;
							// avoids possible tile assignment
					}
					else
					{
						fixIncompleteWay();
					}
				}
				if (futureBounds_ != pastBounds_)
				{
					updateBounds();
				}
				if(isAny(ChangeFlags::TILES_CHANGED | ChangeFlags::MEMBERS_CHANGED))
				{
					identifyPotentialTexChanges();
				}
			}
			if (is(ChangeFlags::RELTABLE_CHANGED))	[[unlikely]]
			{
				if (getRefSE() != CRef::SINGLE_TILE)  [[unlikely]]
				{
					if (way().peekParentRelations())
					{
						validateMemberReltable(&way());
					}
				}
			}
			if (!is(ChangeFlags::DELETED))	[[likely]]
			{
				assignToTiles();
			}
		}

		if (isAny(ChangeFlags::GEOMETRY_CHANGED | ChangeFlags::DELETED))
		{
			ChangeFlags flags = way().flags();
			model().memberChanged(&way(), pastBounds_, futureBounds_,
				(is(ChangeFlags::DELETED) ?
					(ChangeFlags::MEMBERS_CHANGED | ChangeFlags::GEOMETRY_CHANGED) : ChangeFlags::NONE) |
						(flags & (ChangeFlags::GEOMETRY_CHANGED |
							ChangeFlags::BOUNDS_CHANGED)));
			// TODO: Check these flags
		}

		// TODO: TEX changes

		addFlags(ChangeFlags::PROCESSED);
	}

private:
	/// Requires all nodes to be processed (including duplicate nodes
	/// that become unique-location nodes)
	///
	bool computeBounds()
	{
		bool defer = false;
		for(CFeatureStub* nodeStub : members())
		{
			CFeature* node = nodeStub->get();
			CRef ref = node->ref();
			if (ref.isUnknownOrMissing())   [[unlikely]]
			{
				if(ref == CRef::MISSING || mgr_.memberSearchCompleted_)
				{
					node->setRef(CRef::MISSING);
					missingNodes_++;
				}
				else
				{
					// TODO: look up node in index, issue search instruction
					defer = true;
				}
			}
			else
			{
				futureBounds_.expandToInclude(node->xy());
			}
			featureNodes_ += ref.tip().isNull() ? 1 : 0;
		}
		return !defer;
	}

	void fixIncompleteWay() const
	{
		std::span<CFeatureStub*> nodes = members();
		auto originalNodeCount = way().memberCount();
		unsigned validNodeCount = 0;
		for(CFeatureStub* nodeStub : nodes)
		{
			CFeature* node = nodeStub->get();
			CRef ref = node->ref();
			if (!ref.isUnknownOrMissing())
			{
				nodes[validNodeCount++] = node;
			}
		}
		if (validNodeCount == 1)
		{
			// If there's only one valid node remaining,
			// we use it as the second node so a way
			// will always have at least 2 nodes
			nodes[1] = nodes[0];
			validNodeCount++;
		}
		way().setMembers({nodes.data(), validNodeCount});
		setLocalTagWithNumber("geodesk:missing_nodes", missingNodes_);
		addFlags(ChangeFlags::MEMBERS_CHANGED |
			ChangeFlags::WAYNODE_IDS_CHANGED);
	}

	void identifyPotentialTexChanges() const
	{
		Tip wayTip = getRef().tip();
		assert(!wayTip.isNull());
		bool twinTileWay = getRefSE() != CRef::SINGLE_TILE;
		for(CFeatureStub* nodeStub : members())
		{
			CFeature* node = nodeStub->get();
			CRef nodeRef = node->ref();
			Tip nodeTip = nodeRef.tip();
			if (!nodeTip.isNull())
			{
				// feature node
				if (twinTileWay || nodeTip != wayTip) [[unlikely]]
				{
					// node is foreign
					if (!node->isFutureForeign() && !nodeRef.isExported())
					{
						// node will need a TEX (though it may
						// already have one)
						mgr_.texChange(node, false, true);
						node->markAsFutureForeign();
					}
				}
				else
				{
					// TODO: Node doesn't need a TEX
					//  Check if it may have one, add to CM
				}
			}
		}
	}

	ChangedFeature2D& way() const { return wayOrRelation(); }

	int missingNodes_ = 0;
	int featureNodes_ = 0;
};
