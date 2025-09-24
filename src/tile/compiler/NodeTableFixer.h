// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <geodesk/feature/NodeTableIterator.h>
#include "tile/model/TWay.h"
#include "tile/model/TileModel.h"

class NodeTableFixer : public NodeTableIterator
{
public:
	NodeTableFixer(const TWayBody* body, DataPtr newBody) :
		NodeTableIterator(body->handle() - skipReltable(body), 
			newBody - skipReltable(body)),
		adjust_(body->location() + body->anchor() - body->handle())
	{
	}
	
	static int_fast32_t skipReltable(const TWayBody* body)
	{
		return body->constFeature()->flags() & FeatureFlags::RELATION_MEMBER;
			// flag value is 4 == the size of the reltabel pointer
	}
	
	void fix(const TileModel& tile)
	{
		while (next())
		{
			if (!isForeign())
			{
				TReferencedElement* n = tile.getElement(localHandle());
				if (n)
				{
					int32_t relPtr = n->location() + n->anchor() - (currentOfs_ + adjust_);
					MutableDataPtr p(pTile_ + currentOfs_);
					p.putUnsignedShort(static_cast<uint16_t>(relPtr << 1)
						| isLast());
					(p-2).putShort(static_cast<int16_t>(relPtr >> 15));
				}
			}
		}
	}

private:
	int_fast32_t adjust_;
};