// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TNode.h"

#include "MutableFeaturePtr.h"
#include "TileModel.h"

void TNode::write(const TileModel& tile) const
{
	// LOG("  node/%lld", feature().id());
	TFeature::write(tile);
	TRelationTable* rels = parentRelations(tile);
	if (rels)
	{
		MutableDataPtr p(tile.newTileData() + location() + 20);
		p.putInt(rels->location() - location() - 20);
	}
}


TRelationTable* TNode::parentRelations(const TileModel& tile) const
{
	assert(feature().isNode());
	if (!isRelationMember()) return nullptr;

	DataPtr pRelTable = feature().relationTableFast();
	TElement::Handle relsHandle = handle() + DataPtr::nearDelta(pRelTable - node().ptr());
	return tile.getRelationTable(relsHandle);
}

void TNode::setParentRelations(TRelationTable* rels)
{
	assert(rels);
	assert(!isOriginal());
	MutableFeaturePtr pFeature(feature());
	pFeature.setNodeRelations(handle(), rels);
	pFeature.setFlag(FeatureFlags::RELATION_MEMBER, true);
}