// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TIndexTrunk.h"
#include <clarisma/util/log.h>
#include "Layout.h"
#include "TFeature.h"
#include "TIndexLeaf.h"
#include "TileModel.h"


void TIndexTrunk::write(const TileModel& tile) const
{
	int pos = location();
	uint8_t* p = tile.newTileData() + pos;
	TIndexBranch* child = firstChildBranch();
	do
	{
		TIndexBranch* nextChild = child->nextSibling();
		int childLocation;
		if(child->isLeaf())
 		{
			TIndexLeaf* leaf = reinterpret_cast<TIndexLeaf*>(child);
			childLocation = leaf->firstFeature()->location();
		}
		else
		{
			childLocation = child->location();
		}
		assert(childLocation != 0);
		*reinterpret_cast<int32_t*>(p) = (childLocation - pos)
			| (nextChild ? 0 : 1)			// last_item flag
			| (child->isLeaf() ? 2 : 0);	// is_leaf flag
		*reinterpret_cast<Box*>(p + 4) = child->bounds();
		p += 20;
		pos += 20;
		// TODO: Can we safely assume same memory layout of Box? 
		child = nextChild;
	}
	while (child);
#ifndef NDEBUG
	if (pos - location() != size())
	{
		printf("TIndexTrunk stated size = %d, but wrote %d bytes\n",
			size(), pos - location());
		assert(false);
	}
#endif
}


void TIndexTrunk::place(Layout& layout)
{
	layout.place(this);
	TIndexBranch* branch = firstChildBranch();
	do
	{
		if (branch->isLeaf())
		{
			reinterpret_cast<TIndexLeaf*>(branch)->place(layout);
		}
		else
		{
			reinterpret_cast<TIndexTrunk*>(branch)->place(layout);
		}
		branch = branch->nextSibling();
	}
	while (branch);
}
