// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "Feature2dProcessor.h"

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
			if (!tryProcess()) return;
		}

		// We only assign a relation to one or more ChangedTiles
		// when it is "its turn", so the stack linkage is preserved

		if (!is(ChangeFlags::DELETED))	[[likely]]
		{
			assignToTiles();
		}
	}

private:
	bool tryProcess()
	{
		if (!is(ChangeFlags::RELTABLE_LOADED))
		{
			// Only process membership changes if the
			// reltable hasn't been loaded (which already caused
			// memberships to be processed)
			processMembershipChanges();
		}
		normalizeRefs();
		if (is(ChangeFlags::DELETED))	[[unlikely]]
		{
			processDeleted();
		}
		else
		{
			if (isAny(ChangeFlags::MEMBERS_CHANGED | ChangeFlags::BOUNDS_CHANGED))
			{
				if (!computeBounds()) return false;
				if (futureBounds_ == pastBounds_)
				{
					updateBounds();
				}
				else
				{
					clearFlags(ChangeFlags::BOUNDS_CHANGED);
					// Relations are marked BOUNDS_CHANGED during
					// cascades to indicate that their bounds *may*
					// change; once we've actually computed the bounds,
					// we know for sure and can clear the flag
				}
			}
		}

		// Don't assign to tiles yet, needs to happen in process() itself
		// TODO: TEX changes
		addFlags(ChangeFlags::PROCESSED);

		return true;
	}

	bool computeBounds()
	{
		int omittedMembersCount = 0;
		bool defer = false;
		auto members = relation().members();
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

			if(memberType == FeatureType::NODE)
			{
				CRef ref = member->ref();
				if (ref.isUnknownOrMissing())   [[unlikely]]
				{
					if(ref == CRef::MISSING || mgr_.memberSearchCompleted_)
					{
						member->setRef(CRef::MISSING);
						omittedMembersCount++;
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
					memberBounds = member2D->bounds();

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
							omittedMembersCount++;
							members[i] = nullptr;
						}
					}
					else
					{
						memberBounds = member->getFeature(mgr_.store()).bounds();
					}
				}
				futureBounds_.expandToIncludeSimple(memberBounds);
			}
		}
		return !defer;
	}

	ChangedFeature2D& relation() const { return wayOrRelation(); }
};
