// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TRelationTable.h"
#include "TileModel.h"
#include "tile/compiler/RelationTableFixer.h"

void TRelationTable::write(const TileModel& tile) const
{
	uint8_t* p = tile.newTileData() + location();
	TSharedElement::write(p);
	if (needsFixup())
	{
		// Adjust the pointers to local relations
		RelationTableFixer(this, p).fix(tile);
	}
}


bool TRelationTable::operator==(const TRelationTable& other) const
{
	if (hash() != other.hash()) return false;
	if (size() != other.size()) return false;
	if (!needsFixup() && !other.needsFixup())
	{
		// If neither table contains pointers, we can do a 
		// simple byte-wise comparison
		// (The anchor of a reltable is always 0, so no need for check)
		return memcmp(dataStart(), other.dataStart(), size()) == 0;
	}

	// Otherwise, do a tag-by-tag check that normalizes the string handles
	RelationTablePtr pRels = relations();
	RelationTablePtr pOtherRels = other.relations();

	RelationTableIterator iter(handle(), pRels);
	RelationTableIterator otherIter(other.handle(), pOtherRels);
	while (iter.next())
	{
		otherIter.next();		// No need to check for overrun
		if (iter.isForeign())
		{
			// Once foreign features start, we can do a bytewise comparison
			// (Local relations always come before foreign)
			return memcmp(iter.currentPtr(), otherIter.currentPtr(), 
				size() - (iter.currentPtr() - pRels.ptr())) == 0;
		}
		if (otherIter.isForeign()) return false;
			// If this has a local, other must have a local as well
		if (iter.localHandle() != otherIter.localHandle())
		{
			return false;
		}
	}
	return true;
}

