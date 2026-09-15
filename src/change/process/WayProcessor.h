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
		processMembershipChanges();
		normalizeRefs();
		if (is(ChangeFlags::DELETED))	[[unlikely]]
		{
			processDeleted();
		}
		else
		{
			if (isAny(ChangeFlags::GEOMETRY_CHANGED |
				ChangeFlags::WAYNODE_IDS_CHANGED | ChangeFlags::MEMBERS_CHANGED))
			{
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
			}
			assignToTiles();
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
					if (node->isChanged())
					{
						deletedNodes_ += ChangedNode::cast(node)->is(
							ChangeFlags::DELETED) ? 1 : 0;
					}
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

	void fixIncompleteWay()
	{
		std::span<CFeatureStub*> nodes = members();
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

		// TODO: add tag: geodesk:missing_nodes={x}
		//  and mark ChangeFlags::TAGS_CHANGED

		addFlags(ChangeFlags::MEMBERS_CHANGED |
			ChangeFlags::WAYNODE_IDS_CHANGED);
	}

	ChangedFeature2D& way() const { return wayOrRelation(); }

	int missingNodes_ = 0;
	int deletedNodes_ = 0;
	int featureNodes_ = 0;
};
