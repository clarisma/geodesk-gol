// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "TileModel.h"
#include <clarisma/util/DataPtr.h>
#include <clarisma/util/TaggedPtr.h>
#include <geodesk/feature/NodePtr.h>
#include <geodesk/feature/WayPtr.h>
#include <geodesk/feature/RelationPtr.h>
#include "TileReaderBase.h"

class TString;
class TTagTable;
class TRelationTable;


class TileReader : public TileReaderBase<TileReader>
{
public:
	explicit TileReader(TileModel& tile) : tile_(tile), base_(nullptr) {}

	void readTile(Tile tile, TilePtr pTile);

private:
	void readNode(NodePtr node);
	void readWay(WayPtr way);
	void readRelation(RelationPtr relation);
	TString* readString(DataPtr p);
	TTagTable* readTagTable(TagTablePtr pTags);
	TTagTable* readTagTable(FeaturePtr feature);
	TRelationTable* readRelationTable(DataPtr p);
	void readExportTable(DataPtr pTable);
	TElement::Handle handleOf(DataPtr p)
	{
		assert(p.ptr() - base_.ptr() >= 32);
		return static_cast<TElement::Handle>(p.ptr() - base_.ptr());
	}

	TileModel& tile_;
	TilePtr base_;		// TODO: should this be in the base class?

#ifdef _DEBUG
public:
	ElementCounts counts_;
#endif

	friend class TileReaderBase<TileReader>;
};
