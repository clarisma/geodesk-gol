// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <geodesk/feature/RelationTableIterator.h>
#include "tile/model/TRelationTable.h"
#include "tile/model/TileModel.h"

class RelationTableFixer : public RelationTableIterator
{
public:
	RelationTableFixer(const TRelationTable* rels, DataPtr newTable) :
		RelationTableIterator(rels->handle(), newTable),
		adjust_(rels->location() - rels->handle())
	{
	}
	
	void fix(const TileModel& tile)
	{
		while (next())
		{
			if (isForeign()) break;
			TReferencedElement* r = tile.getElement(localHandle());
			assert(r);
			int32_t relPtr = r->location() + 16 - (currentOfs_ + adjust_);
			assert(r->anchor() == 16);	// Anchor is always 16
			MutableDataPtr(pTile_ + currentOfs_).putIntUnaligned(
				(relPtr << 1) | isLast());
		}
	}

private:
	int_fast32_t adjust_;
};