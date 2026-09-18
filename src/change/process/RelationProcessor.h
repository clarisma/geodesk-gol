// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "Feature2dProcessor.h"

// TODO: If a relation changes tiles, all its members need to
//  update their reltables. But we've already processed
//  all its members at this point and assigned them to tiles
//

// TODO: We don't always have both twins of an unchanged relation
//  That can cause problems when the reltable of a twin-tile
//  feature is updated

// TODO: Deletes must always create copies, because relations
//  may be processed out of order, which means we cannot
//  break linkage (ChangeManager::processRelations() uses
//  `next` to maintain its stack of pending relations)
//  Usually, deleted relations will not be processed out
//  of order (since they no longer have parents referencing
//  them), but we cannot be guaranteed that .osc files
//  maintain referential integrity (a relation may contain
//  another relation that has been deleted)

// TODO: Incomplete relations (and their parents) are currently
//  marked as "changed" even if nothing actually changed
//  (The TCA compares past member table to the member table proposed
//  by the osc, and marks the relation as MEMBERS_CHANGED on
//  that basis. If the RelationProcessor later removes any
//  missing member refs, MEMBERS_CHANGED stays set)
//  Better: If members are missing, re-scan past member table
//  against adjusted current, and clear MEMBERS_CHANGED if
//  they are the same). When processing member relations,
//  check if members actually changed and clear flags as needed

class RelationProcessor : public Feature2dProcessor
{
public:
	RelationProcessor(ChangeManager& mgr, ChangedFeature2D& relation) :
		Feature2dProcessor(mgr, relation)
	{
		assert(relation.type() == FeatureType::RELATION);
	}

	void process()
	{
		if (!is(ChangeFlags::PROCESSED))
		{
			if (!tryProcess())
			{
				// Need to defer because we need to search
				// for members whose location is unknown
				model().changedRelations().push(&relation());
				return;
			}

			// TODO: May attempt to process multiple times,
			//  need a way to flag
		}

		// We only assign a relation to one or more ChangedTiles
		// when it is "its turn", so the stack linkage is preserved

		if (!getRef().tip().isNull())	[[likely]]
		{
			assignToTiles();
		}
	}

private:
	bool tryProcess()
	{
		if (relation().id() == 5157108)
		{
			LOGS << feature_.typedId() << " (Version " << feature_.version()
				 << ") at " << feature_.ref() << " / " << feature_.refSE();
		}
		if (!is(ChangeFlags::RELTABLE_LOADED))
		{
			// Only process membership changes if the
			// reltable hasn't been loaded (which already caused
			// memberships to be processed)
			processMembershipChanges();
		}
		if (normalizeRefs() <= 0) [[unlikely]]
		{
			if (!relation().isChangedExplicitly())
			{
				// Missing feature
				addFlags(ChangeFlags::PROCESSED);
				return true;
			}
		}
		// assert(getRef() == CRef::MISSING || !pastBounds_.isEmpty());
		// TODO: check

		if (is(ChangeFlags::DELETED))	[[unlikely]]
		{
			processDeleted();
		}
		else
		{
			if (isAny(ChangeFlags::MEMBERS_CHANGED | ChangeFlags::BOUNDS_CHANGED))
			{
				ensureMembersLoaded();
				if (!computeBounds()) return false;
				if (missingMembers_)	[[unlikely]]
				{
					if (missingMembers_ < members().size())	[[likely]]
					{
						addFlags(ChangeFlags::MEMBERS_CHANGED);
						setLocalTagWithNumber(
							"geodesk:missing_members", missingMembers_);
					}
					else
					{
						// All members missing => delete relation
						addFlags(ChangeFlags::DELETED);
						processDeleted();
						futureBounds_ = pastBounds_;
						// avoids possible tile assignment
					}
				}
				if (relation().removedRefcyleCount())	[[unlikely]]
				{
					addFlags(ChangeFlags::MEMBERS_CHANGED);
					// TODO: Flag change needed? Done earlier?
					setLocalTagWithNumber("geodesk:removed_refcycles",
						relation().removedRefcyleCount());
				}
				if (futureBounds_ != pastBounds_)
				{
					updateBounds();
					if (is(ChangeFlags::TILES_CHANGED))
					{
						// TODO: If this relation changes tiles, all its
						//  members must update their reltables
					}
				}
				else
				{
					clearFlags(ChangeFlags::BOUNDS_CHANGED);
					// Relations are marked BOUNDS_CHANGED during
					// cascades to indicate that their bounds *may*
					// change; once we've actually computed the bounds,
					// we know for sure and can clear the flag
				}
				identifyPotentialTexChanges();
			}
		}

		if (relation().id() == 5157108)
		{
			LOGS << feature_.typedId() << " (Version " << feature_.version()
				 << ") at " << feature_.ref() << " / " << feature_.refSE();
		}

		// Don't assign to tiles yet, needs to happen in process() itself
		// TODO: TEX changes
		addFlags(ChangeFlags::PROCESSED);

		return true;
	}

	bool computeBounds()
	{
		bool defer = false;
		auto members = relation().members();
		for(int i=0; i<members.size(); i++)
		{
			if(members[i] == nullptr)   [[unlikely]]
			{
				// The member has been determined missing in an
				// earlier attempt, and replaced with null
				missingMembers_++;
				continue;
			}
			CFeature* member = members[i]->get();
			FeatureType memberType = member->type();

			if(memberType == FeatureType::NODE)
			{
				CRef ref = member->ref();
				if (ref.isUnknownOrMissing())   [[unlikely]]
				{
					if(ref == CRef::MISSING || mgr_.memberSearchCompleted_)
					{
						member->setRef(CRef::MISSING);
						missingMembers_++;
						members[i] = nullptr;
					}
					else
					{
						// TODO: look up node in index, issue search instruction
						defer = true;
					}
				}
				else
				{
					futureBounds_.expandToInclude(member->xy());
				}
			}
			else
			{
				Box memberBounds;
				if(member->isChanged())	[[unlikely]]
				{
					ChangedFeature2D* member2D = ChangedFeature2D::cast(member);
					if(!member2D->is(ChangeFlags::PROCESSED))	[[unlikely]]
					{
						// Member way or relation has not been
						// processed yet
						if (memberType == FeatureType::RELATION)  [[unlikely]]
						{
							// TODO: This is inefficient; if multiple
							//  relations have this child relation as
							//  member, we'll attempt to process it
							//  multiple times; better to set a flag
							//  that is has been attempted during
							//  current cycle
							if (!RelationProcessor(mgr_, *member2D).
								tryProcess())
							{
								defer = true;
								continue;
							}
						}
						else
						{
							defer = true;
							continue;
						}
					}
					if(member2D->ref() == CRef::MISSING)	[[unlikely]]
					{
						missingMembers_++;
						members[i] = nullptr;
						continue;
					}
					model().ensureBounds(member2D);
					// TODO: This could be avoided if we always
					//  load the bounds when we create a ChangedFeature
					//  for an existing way
					memberBounds = member2D->bounds();
					assert(!memberBounds.isEmpty());

					// TODO: Do we need this?
					//  memberTilesChanged |= member2D->is(
					//  	ChangeFlags::TILES_CHANGED);
				}
				else
				{
					int res = mgr_.normalizeRefs(member);
					if (res <= 0)	[[unlikely]]
					{
						if (res < 0)
						{
							defer = true;
						}
						else
						{
							missingMembers_++;
							members[i] = nullptr;
						}
					}
					else
					{
						memberBounds = member->getFeature(mgr_.store()).bounds();
					}
				}
				assert(!memberBounds.isEmpty());
				futureBounds_.expandToIncludeSimple(memberBounds);
			}
		}
		return !defer;
	}

	void identifyPotentialTexChanges() const
	{
		Tip relTip = getRef().tip();
		assert(!relTip.isNull());
		Tip relTipSE = getRefSE().tip();
		bool anyMembersForeign = false;
		for(CFeatureStub* memberStub : members())
		{
			if (!memberStub) continue;	// skip removed members
			CFeature* member = memberStub->get();
			CRef memberRef = member->ref();
			Tip memberTip = memberRef.tip();
			assert(!memberTip.isNull());
			CRef memberRefSE;
			if (member->type() == FeatureType::NODE)
			{
				memberRefSE = CRef::SINGLE_TILE;
			}
			else
			{
				memberRefSE = member->refSE();
			}
			Tip memberTipSE = memberRefSE.tip();

			if (memberTip != relTip || memberTipSE != relTipSE)
			{
				// member is foreign
				member->markAsFutureForeign();
				if (!memberRef.isExported())
				{
					// member will need a TEX (though it may
					// already have one)
					mgr_.texChange(member, false, true);
				}
				if (!memberTipSE.isNull())	[[unlikely]]
				{
					// TODO: Do we really need to check separately?
					//  Can a dual-tile feature be exported from one tile,
					//  but not the other?
					if (!memberRefSE.isExported())
					{
						// member will need a TEX in its SE tile
						// (though it may already have one)
						mgr_.texChange(member, true, true);
					}
				}
				anyMembersForeign = true;
			}
			else
			{
				// TODO: Member doesn't need a TEX
				//  Check if it may have one, add to CM
			}

			if (is(ChangeFlags::TILES_CHANGED))
			{
				// If the relation's tiles changed, we need to
				// force each member to update its reltable
				ensureReltableChanged(member);
			}
		}

		if (anyMembersForeign)
		{
			relation().markAsFutureForeign();
			if (!getRef().isExported())
			{
				// relation will need a TEX (though it may
				// already have one)
				mgr_.texChange(&relation(), false, true);
			}
			CRef refSE = getRefSE();
			if (!refSE.tip().isNull())	[[unlikely]]
			{
				// TODO: Do we really need to check separately?
				//  Can a dual-tile feature be exported from one tile,
				//  but not the other?
				if (!refSE.isExported())
				{
					// relation will need a TEX in its SE tile
					// (though it may already have one)
					mgr_.texChange(&relation(), true, true);
				}
			}
		}
		else
		{
			// TODO: relation's TEX may need to be dropped
		}

		// TODO: Consolidate texChange code
	}

	// TODO: cleanup
	void ensureReltableChanged(CFeature* member) const
	{
		ChangedFeatureBase* changed;
		if (member->isChanged())
		{
			changed = ChangedFeatureBase::cast(member);
			assert(changed->is(ChangeFlags::PROCESSED));
			model().getParentRelations(changed);
			changed->addFlags(ChangeFlags::RELTABLE_CHANGED);
			return;
		}
		changed = model().getChanged(member);
		assert(!changed->is(ChangeFlags::PROCESSED));
		// Need to remove feature from stacks so it doesn't get
		// processed in the second round of processing
		ChangedFeatureBase* popped;
		switch (member->type())
		{
		case FeatureType::NODE:
			popped = model().changedNodes().pop();
			break;
		case FeatureType::WAY:
			popped = model().changedWays().pop();
			break;
		case FeatureType::RELATION:
			popped = model().changedRelations().pop();
			break;
		default:
			popped = nullptr;
			assert(false);
		}
		assert(popped == changed);
		model().getParentRelations(changed);
		assert(changed->is(ChangeFlags::RELTABLE_LOADED));
		changed->addFlags(ChangeFlags::RELTABLE_CHANGED |
			ChangeFlags::PROCESSED);

		// TODO: This duplicates assignToTiles
		CRef ref;
		Tip tip;
		if (!changed->isNode())
		{
			ref = changed->refSE();
			tip = ref.tip();
			if(!tip.isNull())   [[unlikely]]
			{
				mgr_.getChangedTile(tip)->addChanged(model().copy(changed));
			}
		}
		ref = changed->ref();
		tip = ref.tip();

		if (changed->isNode())
		{
			mgr_.getChangedTile(tip)->changedNodes().push(
				ChangedNode::cast(changed));
		}
		else
		{
			mgr_.getChangedTile(tip)->addChanged(changed);
		}
	}

	ChangedFeature2D& relation() const { return wayOrRelation(); }

	int missingMembers_ = 0;
};
