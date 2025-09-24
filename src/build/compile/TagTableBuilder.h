// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <vector>
#include <clarisma/data/Span.h>
#include "tag/TagTableModel.h"
#include "tile/model/TString.h"

class AreaClassifier;
class StringCatalog;
class TileModel;
class TTagTable;

class TagTableBuilder : public TagTableModel
{
public:
	TagTableBuilder(TileModel& tile, const AreaClassifier& areaClassifier, const StringCatalog& strings) :
		tile_(tile),
		areaClassifier_(areaClassifier),
		strings_(strings)
	{
	}

	TTagTable* getTagTable(ByteSpan tags, bool determineIfArea);
	TTagTable* getTagTable(bool determineIfArea);

private:
	TileModel& tile_;
	const StringCatalog& strings_;
	const AreaClassifier& areaClassifier_;
};
