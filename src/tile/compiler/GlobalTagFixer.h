// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <geodesk/feature/GlobalTagIterator.h>
#include "tile/model/TTagTable.h"
#include "tile/model/TileModel.h"

class GlobalTagFixer : public GlobalTagIterator
{
public:
	GlobalTagFixer(const TTagTable* tags, DataPtr newTags) :
		GlobalTagIterator(tags->handle(), TagTablePtr(newTags, false)),
		adjust_(tags->location() + tags->anchor() - tags->handle())
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
				int_fast32_t valOfs = ofs_ - 4;
				MutableDataPtr(pTile_ + valOfs).putIntUnaligned(
					valStr->location() - (valOfs + adjust_));
			}
		}
	}


private:
	int_fast32_t adjust_;
};