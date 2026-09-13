// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TTagTable.h"
#include <clarisma/util/log.h>
#include <clarisma/util/StringBuilder.h>
#include "Layout.h"
#include "TileModel.h"
#include "tile/compiler/IndexSettings.h"
#include "tile/compiler/GlobalTagFixer.h"
#include "tile/compiler/LocalTagFixer.h"

void TTagTable::write(const TileModel& tile) const
{
	//LOG("Writing tags %s", toString(tile).c_str());
	uint8_t* p = tile.newTileData() + location();
	TSharedElement::write(p);
	if (needsFixup())
	{
		LocalTagFixer(this, p + anchor()).fix(tile);
		GlobalTagFixer(this, p + anchor()).fix(tile);
	}
}

// TODO: move this to compiler
uint32_t TTagTable::assignIndexCategory(const IndexSettings& indexSettings)
{
	int maxIndexedKey = indexSettings.maxIndexedKey();
	int category = 0;
	uint32_t indexBits = 0;
	DataPtr p = data();		// anchored pointer
	int keyWithLastFlag;
	do
	{
		uint16_t keyBits = p.getUnsignedShort();
		keyWithLastFlag = keyBits >> 2;
		int keyCategory = indexSettings.getCategory(keyWithLastFlag & 0x1FFF);
		if (keyCategory > 0)
		{
			if (category != 0)
			{
				category = TIndex::MULTI_CATEGORY;
			}
			else
			{
				category = keyCategory;
			}
			assert (keyCategory >= 1 && keyCategory <= TIndex::MAX_CATEGORIES);
			indexBits |= (1 << (keyCategory - 1));
		}
		p += 4 + (keyBits & 2);
	}
	while (keyWithLastFlag < maxIndexedKey);
	category_ = category;
	return indexBits;
}


void TTagTable::placeString(Layout& layout, TElement::Handle strHandle)
{
	// TODO: handle to new strings
	TileModel& tile = layout.tile();
	TString* str = tile.getKeyString(strHandle);
		// TODO: not all are key strings, special lookup may be wasteful
	if (str == nullptr)
	{
		LOGS << "Failed to get string for handle " << strHandle;
	}
	assert(str);
	if (str->location() == 0)
	{
		layout.addBodyElement(str);
	}
}


void TTagTable::placeStrings(Layout& layout) const
{
	LocalTagIterator localTags(handle(), tags());
	while (localTags.next())
	{
		placeString(layout, localTags.keyStringHandle());
		if (localTags.hasLocalStringValue())
		{
			placeString(layout, localTags.stringValueHandleFast());
		}
	}

	GlobalTagIterator globalTags(handle(), tags());
	while (globalTags.next())
	{
		if (globalTags.hasLocalStringValue())
		{
			placeString(layout, globalTags.stringValueHandleFast());
		}
	}
}


bool TTagTable::operator==(const TTagTable& other) const
{
	if (hash() != other.hash()) return false;
	if (size() != other.size() || anchor() != other.anchor()) return false;
	if (!needsFixup() && !other.needsFixup())
	{
		// If neither table contains pointers, we can do a 
		// simple byte-wise comparison

		// Two tag tables that are bytewise identical can still represent
		// different tags, depending on whether the bytes are interpreted
		// as global or local tags, hence it is important that we check 
		// the anchor, as well

		return memcmp(dataStart(), other.dataStart(), size()) == 0;
	}

	// Otherwise, do a tag-by-tag check that normalizes the string handles
	TagTablePtr pTags = tags();
	TagTablePtr pOtherTags = other.tags();
	
	GlobalTagIterator globalTags(handle(), pTags);
	GlobalTagIterator otherGlobalTags(other.handle(), pOtherTags);
	while (globalTags.next())
	{
		otherGlobalTags.next();
			// No risk of an overrun, since we implicitly checked the last-flag
			// by comparing the keyBits; if the other's global tags were at
			// the end, we would have already bailed because the bits don't match
		if (globalTags.keyBits() != otherGlobalTags.keyBits()) return false;
		if (globalTags.hasLocalStringValue())
		{
			if (globalTags.stringValueHandleFast() !=
				otherGlobalTags.stringValueHandleFast())
			{
				return false;
			}
		}
		else
		{
			if (globalTags.value() != otherGlobalTags.value()) return false;
		}
	}

	LocalTagIterator localTags(handle(), pTags);
	LocalTagIterator otherLocalTags(other.handle(), pOtherTags);
	while (localTags.next())
	{
		otherLocalTags.next();
			// Since we've checked if the flags of the previous tags matched, 
			// we can rest assured that we're not reading beyond the table
		if (localTags.flags() != otherLocalTags.flags()) return false;

		//LOG("This key handle = %d", localTags.keyStringHandle());
		//LOG("Other key handle = %d", otherLocalTags.keyStringHandle());

		if (localTags.keyStringHandle() != otherLocalTags.keyStringHandle())
		{
			return false;
		}
		if (localTags.hasLocalStringValue())
		{
			if (localTags.stringValueHandleFast() !=
				otherLocalTags.stringValueHandleFast())
			{
				return false;
			}
		}
		else
		{
			if (localTags.value() != otherLocalTags.value()) return false;
		}
	}
return true;
}

template <typename T>
static void writeTagValue(const TileModel& tile, StringBuilder& s, T& tag)
{
	s.writeByte('=');
	if (tag.hasStringValue())
	{
		if (tag.hasLocalStringValue())
		{
			int_fast32_t strHandle = tag.stringValueHandleFast();
			s << *tile.getString(strHandle)->string();
		}
		else
		{
			s.writeByte('#');
			s << tag.value();
		}
	}
	else
	{
		if (tag.hasWideValue())
		{
			s << TagValues::doubleFromWideNumber(tag.value());
		}
		else
		{
			s << TagValues::intFromNarrowNumber(tag.value());
		}
	}
}

std::string TTagTable::toString(const TileModel& tile) const
{
	StringBuilder s;
	TagTablePtr pTags = tags();
	bool first = true;
	GlobalTagIterator globalTags(handle(), pTags);
	while (globalTags.next())
	{
		if(!first) s.writeByte(',');
		uint32_t k = globalTags.key();
		if (k == 0) continue;		// "no globals" marker
		s.writeByte('#');
		s << k;
		writeTagValue(tile, s, globalTags);
		first = false;
	}
	LocalTagIterator localTags(handle(), pTags);
	while (localTags.next())
	{
		if (!first) s.writeByte(',');
		int_fast32_t k = localTags.keyStringHandle();
		s << *tile.getKeyString(k)->string();
		writeTagValue(tile, s, localTags);
		first = false;
	}
	return s.toString();
}
