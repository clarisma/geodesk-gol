// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TSharedElement.h"
#include <geodesk/feature/RelationTablePtr.h>

using namespace geodesk;
class TileModel;

class TRelationTable : public TSharedElement
{
public:
	TRelationTable(Handle handle, const uint8_t* data, uint32_t size, uint32_t hash) :
		TSharedElement(TYPE, handle, data, size, Alignment::WORD, hash)
	{
	}

	RelationTablePtr relations() const { return RelationTablePtr(data()); }

	bool operator==(const TRelationTable& other) const;

	void write(const TileModel& tile) const;

	static constexpr TElement::Type TYPE = TElement::Type::RELTABLE;
};