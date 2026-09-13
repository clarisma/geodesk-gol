// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TFeature2D.h"
#include "Layout.h"
#include "MutableFeaturePtr.h"

void TFeature2D::write(const TileModel& tile) const
{
	TFeature::write(tile);
	MutableDataPtr p(tile.newTileData() + location() + 28);
	const TFeatureBody* body = constBody();
	p.putInt(body->location() + body->anchor() - location() - 28);
}


TRelationTable* TFeature2D::parentRelations(TileModel& tile) const
{
	if (!isRelationMember()) return nullptr;
	TFeatureBody* body = const_cast<TFeature2D*>(this)->body();
	DataPtr ppRelTable = body->data() - 4;
	int_fast32_t ofs = body->handle() - 4;
	Handle relsHandle = ofs + ppRelTable.getIntUnaligned();
	return tile.getRelationTable(relsHandle);
}


void TFeature2D::setParentRelations(TRelationTable* rels)
{
	assert(rels);
	assert(body()->anchor() >= 4);
	MutableDataPtr ppRelTable(body()->data() - 4);
	int_fast32_t ofs = body()->handle() - 4;
	ppRelTable.putInt(rels->handle() - ofs);
	MutableFeaturePtr(feature()).setFlag(FeatureFlags::RELATION_MEMBER, true);
}


void TFeatureBody::fixRelationTablePtr(uint8_t* pBodyStart, const TileModel& tile) const
{
	MutableDataPtr p(pBodyStart + anchor() - 4);
	TRelationTable* rels = tile.getRelationTable(handle() - 4 + p.getIntUnaligned());
	assert(rels);
	int ofs = location() + anchor() - 4;
	p.putIntUnaligned(rels->location() - ofs);
}