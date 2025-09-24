// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <geodesk/feature/LocalTagIterator.h>
#include "tile/model/TTagTable.h"
#include "tile/model/TileModel.h"

class LocalTagFixer : public LocalTagIterator
{
public:
	LocalTagFixer(const TTagTable* tags, DataPtr newTags) :
		LocalTagIterator(tags->handle(), TagTablePtr(newTags, tags->hasLocalTags())),
		adjust_(tags->location() + tags->anchor() - tags->handle()),
		newOriginOfs_((tags->location() + tags->anchor()) & 0xffff'fffc)
	{
	}
	
	void fix(const TileModel& tile)
	{
		while (next())
		{
			if (hasLocalStringValue())
			{
				TString* valStr = tile.getString(stringValueHandleFast());
				assert(valStr);
				MutableDataPtr(pTile_ + ofs_).putIntUnaligned(
					valStr->location() - (ofs_ + adjust_));
			}
			TString* keyStr = tile.getKeyString(keyStringHandle());
			assert(keyStr);
			int_fast32_t keyOfs = ofs_ + 2 + (keyBits_ & 2);
			int_fast32_t keyPtr = keyStr->location() - newOriginOfs_;
			assert((keyPtr & 3) == 0);	// must be 4-byte aligned pointer
			MutableDataPtr(pTile_ + keyOfs).putIntUnaligned((keyPtr << 1) | flags());
		}
	}

private:
	int_fast32_t adjust_;
	int_fast32_t newOriginOfs_;
};