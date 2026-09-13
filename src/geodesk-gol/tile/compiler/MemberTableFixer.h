// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <geodesk/feature/MemberTableIterator.h>
#include "tile/model/TRelation.h"
#include "tile/model/TileModel.h"

// TODO: Check if this class can handle negative offsets
//  The handle for a way's body could be 4 (first assigned handle),
//  so offsets in the feature-node table could be negative, since
//  this table is placed ahead of the anchor (which is the handle)
//
class MemberTableFixer : public MemberTableIterator
{
public:
	MemberTableFixer(const TRelationBody* body, DataPtr newTable) :
		MemberTableIterator(body->handle(), newTable),
		adjust_(body->location() + static_cast<int>(body->anchor()) - body->handle())
	{
	}
	
	void fix(const TileModel& tile)
	{
		while (next())
		{
			if (!isForeign())
			{
                TReferencedElement* m = tile.getElement(localHandle());
				assert(m);
				int32_t relPtr = m->location() + m->anchor() -
					((currentOfs_ + adjust_) & 0xffff'fffc);
				MutableDataPtr(pTile_ + currentOfs_).putIntUnaligned(
					(relPtr << 1) | (member_ & 7));
			}
			if (hasDifferentRole() && hasLocalRole())
			{
				TString* role = tile.getString(localRoleHandleFast());
				assert(role);
				int32_t relPtr = role->location() - (currentRoleOfs_ + adjust_);
				/*
				LOGS << "Adjusting local-role ref to " << role->string()
					<< " at loc " << role->location()
					<< ": relPtr = " << relPtr
					<< ", true ofs = " << (currentRoleOfs_ + adjust_) << '\n';
				*/
				MutableDataPtr(pTile_ + currentRoleOfs_).putIntUnaligned(relPtr << 1);
			}
		}
	}

private:
	int_fast32_t adjust_;
};