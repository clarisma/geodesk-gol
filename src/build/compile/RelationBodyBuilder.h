// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <string_view>
#include <span>
#include "tile/model/Membership.h"
#include "RelationMember.h"

class TileModel;
class TFeature;
class TRelationBody;
class TRelationTable;
class TString;

using namespace geodesk;

// TODO: Go back to earlier design, we can safely reduce arena allocs in most
// cases (need to add a check to the Arena), and even if we cannot trim back,
// the wasted space will still be more efficient

class RelationBodyBuilder
{
public:
	explicit RelationBodyBuilder(std::span<RelationMember> members) :
		members_(members),
		currentMember_(0)
	{
	}

	void addLocal(TFeature* local, Role role)
	{
		assert(currentMember_ < members_.size());
		members_[currentMember_++] = RelationMember(local, {}, role);
		prevAltRef_ = ForeignFeatureRef();
	}

	void addForeign(ForeignFeatureRef ref, ForeignFeatureRef altRef, Role role)
	{
		assert(currentMember_ < members_.size());
		if (!prevAltRef_.isNull())
		{
			assert(currentMember_ > 0);
			RelationMember& prev = members_[currentMember_-1];
			assert(!prev.foreign.isNull());
			if (altRef == prevAltRef_)
			{
				prev.foreign = prevAltRef_;
				ref = altRef;
				altRef = {};
			}
			else if (ref == prevAltRef_)
			{
				prev.foreign = prevAltRef_;
				altRef = {};
			}
		}
		if (!altRef.isNull())
		{
			// The member lives in two tiles, so we need to pick the ideal one
			// First, we'll try the obvious: Use the same TIP as the previous TIP
			if (altRef.tip == prevStagedTip_)
			{
				ref = altRef;
			}
			else if (ref.tip == prevStagedTip_)
			{
				altRef = {};
			}

			// TODO: If one tile is covered by the current tile, and the other is not
			// (possible if the relation itself is multi-tile), pick the TIP of the 
			// covered tile; this is useful in situations when a user has only a partial
			// tileset, with only one of the relation's tiles -- it is much more likely
			// that the covered will be present
			// TODO: To implement this properly, would *always* have to give the covered
			// tile precendence 
			// Need acess to TileCatalog (TIP -> Tile -> Box)

			// Best approach: Let caller (CompilerWorker) decide, because it
			// has all the requisite resources

			// Otherwise, we'll defer the member, so we can choose the TIP 
			// based on the TIP(s) of the next member 
		}
		members_[currentMember_++] = RelationMember(nullptr, ref, role);
		prevStagedTip_ = ref.tip;
		prevAltRef_ = altRef;
	}

	void build(TileModel& tile, TRelationBody* body, Membership* firstParent);

private:
	std::span<RelationMember> members_;
	ForeignFeatureRef prevAltRef_;
	Tip prevStagedTip_;
	int currentMember_;
	#ifdef GOL_BUILD_STATS
public:
	int memberCount = 0;
	int foreignMemberCount = 0;
	int wideTexMemberCount = 0;
	#endif
};

