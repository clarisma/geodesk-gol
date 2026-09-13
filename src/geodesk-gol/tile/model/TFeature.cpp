// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TFeature.h"
#include "TFeature2D.h"
#include "TNode.h"
#include "Layout.h"
#include "Membership.h"
#include "MutableFeaturePtr.h"
#include "TileModel.h"

TTagTable* TFeature::tags(const TileModel& tile) const
{
	/*  // TODO: Restore this code, remove code below
	int32_t tagPtr = (data() + 8).getInt() & 0xffff'fffe;
	assert(tagPtr != 0);
	return tile.getTags(handle() + 8 + tagPtr);
	*/

	int32_t tagPtr = (data() + 8).getInt() & 0xffff'fffe;
	Handle tagsHandle = handle() + 8 + tagPtr;
	TElement* tags = tile.getElement(tagsHandle);
	if(tags->type() != TTagTable::TYPE)
	{
		LOGS << typedId() << " has a bad tag table (tags handle = " << tagsHandle << ")";
		LOGS << "  Handle of " << typedId() << " = " << handle();
	}

	return TElement::cast<TTagTable>(tags);
}


TRelationTable* TFeature::parentRelations(TileModel& tile) const
{
	if (feature().isNode())
	{
		return static_cast<const TNode*>(this)->parentRelations(tile);
	}
	else
	{
		return static_cast<const TFeature2D*>(this)->parentRelations(tile);
	}
}

// TODO: clean this up
void TFeature::placeRelationTable(Layout& layout)
{
	TileModel& tile = layout.tile();
	assert(isRelationMember());
	TRelationTable* relTable = parentRelations(tile);
	assert(relTable);
	if (relTable->location() == 0)
	{
		layout.addBodyElement(relTable);
	}
}


MutableFeaturePtr TFeature::makeMutable(TileModel& tile)
{
	if (isOriginal())
	{
		int anchor = typeCode() == 0 ? 8 : 16;	// TODO: needed??
		int size = typeCode() == 0 ? 24 : 32;   // TODO: needed??
		uint8_t* dataStart = tile.arena().alloc(size, alignof(uint32_t));
		memcpy(dataStart, data_ - anchor, size);
		data_ = dataStart + anchor;
		setOriginal(false);
	}
	return MutableDataPtr(data_);
}


void TFeature::write(const TileModel& tile) const
{
	int_fast32_t a = anchor();
	MutableDataPtr p(tile.newTileData() + location());
	memcpy(p, feature().ptr() - a, 8 + a);
	p += a;
	p.putInt((p.getInt() & ~1) | (isLast() ? 1 : 0));
		// set the is_last flag bit
		// TODO: may change
	p += 8;
	TTagTable* tags = this->tags(tile);
	p.putInt((tags->location() + tags->anchor() - 
		location() - a - 8) | (tags->hasLocalTags() ? 1 : 0));
}


void TFeature::addMembership(Membership* membership)
{
	membership->sortedInsert(&firstMembership_);
}