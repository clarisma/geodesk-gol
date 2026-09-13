// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TSharedElement.h"
#include "TIndex.h"
#include "TString.h"
#include <geodesk/feature/TagTablePtr.h>

using namespace geodesk;
class IndexSettings;
class Layout;
class TileModel;


class TTagTable : public TSharedElement
{
public:
	TTagTable(Handle handle, const uint8_t* data, uint32_t size, 
		uint32_t hash, uint32_t anchor) :
		TSharedElement(TYPE, handle, data, size, Alignment::WORD, hash, anchor)
	{
		setCategory(TIndex::UNASSIGNED_CATEGORY);
	}

	TagTablePtr tags() const { return TagTablePtr(data(), hasLocalTags()); }
	bool hasLocalTags() const { return anchor() != 0; }
	void placeStrings(Layout& layout) const;
	void write(const TileModel& tile) const;
	uint32_t assignIndexCategory(const IndexSettings& indexSettings);
	std::string toString(const TileModel& tile) const;
	bool isArea(bool forRelation) const
	{
		assert(isBuilt());
		return flags() & (forRelation ? RELATION_AREA_TAGS : WAY_AREA_TAGS);
	}

	TTagTable* nextTags() const
	{
		assert(next_ == nullptr || next_->type() == Type::TAGS);
		return reinterpret_cast<TTagTable*>(next_);
	}

	// bool equals(const TTagTable* other) const;

	bool operator==(const TTagTable& other) const;
	
	static constexpr TElement::Type TYPE = TElement::Type::TAGS;

private:
	static void placeString(Layout& layout, TElement::Handle strHandle);
};