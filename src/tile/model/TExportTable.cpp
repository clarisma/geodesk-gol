// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TExportTable.h"
#include "TileModel.h"

void TExportTable::write(const TileModel& tile) const
{
	int ofs = location();
	MutableDataPtr p(tile.newTileData() + ofs);
	uint32_t count = size() / 4 - 1;
	assert(count);
	p.putUnsignedInt(count);
	if(features_)
	{
		for(int i=0; i < count; i++)
		{
			ofs += 4;
			p += 4;
			assert(features_[i]);
			p.putInt(features_[i]->target() - ofs);
		}
	}
	else
	{
		TypedFeatureId* pTypedId = typedIds_;
		assert(pTypedId);
		for(int i=0; i < count; i++)
		{
			ofs += 4;
			p += 4;
			TFeature* f = tile.getFeature(*pTypedId++);
			assert(f);
			p.putInt(f->target() - ofs);
		}
	}
}
