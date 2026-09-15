// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "change/ChangeManager.h"
#include "change/model/ChangedTile.h"

using namespace geodesk;

// TODO: A feature status change is a tile change, makes
//  cascading logic easier: cascade geometry, tiles
// TODO: But don't cascade new nodes; cascades are only
//  for existing nodes

class FeatureProcessor
{
public:
	FeatureProcessor(ChangeManager& mgr, ChangedFeatureBase& feature) :
		feature_(feature),
		mgr_(mgr)
	{
	}

	void processMembershipChanges() const
	{
		// TODO: Do we need to guard against the reltable already
		//  being loaded? (Changes and actual table are unioned)

		const MembershipChange* changes = feature_.membershipChanges();
		if (changes)    [[unlikely]]
		{
			CRef ref = feature_.ref();
			if (!ref.canGetFeature() && feature_.type() != FeatureType::NODE)
			{
				ref = feature_.refSE();
			}
			feature_.setParentRelations(model().getRelationTable(ref, changes));
		}
	}

protected:
	ChangeModel& model() const { return mgr_.model(); }

	bool is(ChangeFlags flags) const noexcept
	{
		return feature_.is(flags);
	}

	bool isAny(ChangeFlags flags) const noexcept
	{
		return feature_.isAny(flags);
	}

	void addFlags(ChangeFlags flags) const
	{
		feature_.addFlags(flags);
	}

	void clearFlags(ChangeFlags flags) const
	{
		feature_.clearFlags(flags);
	}

	CRef getRef() const noexcept { return feature_.ref(); }
	void setRef(CRef ref) const noexcept { feature_.setRef(ref); }

	void remove(bool fromSE) const
	{
		mgr_.remove(&feature_, fromSE);
	}

	ChangedFeatureBase& feature_;
	ChangeManager& mgr_;
};
