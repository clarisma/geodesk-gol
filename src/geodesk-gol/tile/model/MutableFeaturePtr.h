// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TRelationTable.h"
#include "TTagTable.h"
#include <geodesk/feature/FeaturePtr.h>

class MutableFeaturePtr : public FeaturePtr
{
public:
	MutableFeaturePtr(DataPtr p) : FeaturePtr(p) {}

	void setFlag(int flag, bool b)
	{
		// TODO: THis may change in 2.0 with 12 instead of 8 flags
		int bits = p_.getInt();
		int newBits = b ? (bits | flag) : (bits & ~flag);
		MutableDataPtr(p_).putInt(newBits);
	}

	void setTags(TElement::Handle handle, TTagTable* tags)
	{
		MutableDataPtr(p_ + 8).putInt(tags->handle() - handle - 8 +
			(tags->hasLocalTags() ? 1 : 0));
	}

	void setBounds(const Box& bounds)
	{
		assert(!isNode());
		MutableDataPtr(p_ - 16).putBytes(&bounds, 16);
	}

	void setNodeXY(const Coordinate& xy)
	{
		assert(isNode());
		MutableDataPtr(p_ - 8).putInt(xy.x);
		MutableDataPtr(p_ - 4).putInt(xy.y);
	}

	void setNodeRelations(TElement::Handle handle, TRelationTable* rels)
	{
		assert(isNode());
		assert(rels);
		MutableDataPtr(p_ + 12).putInt(rels->handle() - handle - 12);
	}
};

