// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cassert>
#include <clarisma/util/log.h>
#include <clarisma/util/MutableDataPtr.h>
#include "tile/model/TTagTable.h"
#include "TagTableHasher.h"

// Caution: Offsets can be negative, since we place local keys
// ahead of the handle (which is the anchor point)
// The handle could be 4, if the tagtable is the first element
// placed into the model
// We cannot assume 0 or negative offsets have any special meaning,
// so we cannot set prevKeyOfs_ to 0 to indicate "no tags have yet been
// written"

class TagTableWriter
{
public:
	TagTableWriter(TElement::Handle handle, DataPtr p) :
		pTile_(p - handle),
		ofs_(handle),
		prevKeyOfs_(handle),
		tableOfs_(handle),
		originOfs_(handle & 0xffff'fffc)
	{
	}

	explicit TagTableWriter(TTagTable* tags) :
		TagTableWriter(tags->handle(), tags->data()) {}

	DataPtr ptr() const { return pTile_ + ofs_; }
	size_t hash() const { return hasher_.hash(); }

	void writeLocalTag(int valueFlags, TString* key, uint32_t value)
	{
		writeLocalKey(valueFlags, key);
		if (valueFlags & 2)
		{
			ofs_ -= 4;
			(pTile_ + ofs_).putUnsignedIntUnaligned(value);
		}
		else
		{
			ofs_ -= 2;
			(pTile_ + ofs_).putUnsignedShort(static_cast<uint16_t>(value));
		}
		hasher_.addValue(value);
	}

	void writeLocalTag(TString* key, TString* value)
	{
		writeLocalKey(3, key);
		ofs_ -= 4;
		writeStringValue(value);
	}

	void writeGlobalTag(int valueFlags, uint32_t keyCode, uint32_t value)
	{
		//LOG("Writing global tag %d=%d", keyCode, value);
		writeGlobalKey(valueFlags, keyCode);
		//assert((valueFlags & 3) != 3);
		if (valueFlags & 2)
		{
			(pTile_ + ofs_).putUnsignedIntUnaligned(value);
			ofs_ += 4;
		}
		else
		{
			(pTile_ + ofs_).putUnsignedShort(static_cast<uint16_t>(value));
			ofs_ += 2;
		}
		hasher_.addValue(value);
	}

	void writeGlobalTag(uint32_t keyCode, TString* value)
	{
		writeGlobalKey(3, keyCode);
		writeStringValue(value);
		ofs_ += 4;
	}

	void endLocalTags()
	{
		if (prevKeyOfs_ < tableOfs_)
		{
			MutableDataPtr pKey = pTile_ + prevKeyOfs_;
			pKey.putUnsignedShort(pKey.getUnsignedShort() | 4);
			ofs_ = tableOfs_;
		}
	}

	void endGlobalTags()
	{
		assert(prevKeyOfs_ >= tableOfs_); // At least one global tag must have been written
		MutableDataPtr pKey = pTile_ + prevKeyOfs_;
		pKey.putUnsignedShort(pKey.getUnsignedShort() | 0x8000);
	}

	
private:
	void writeLocalKey(int valueFlags, TString* key)
	{
		assert((valueFlags & 3) == valueFlags && "Only string-flag and wide-flag may be set");
		ofs_ -= 4;
		prevKeyOfs_ = ofs_;
		int32_t keyHandle = key->handle() & 0xffff'fffc;
		// When updating, a new tag table may use an existing string
		// as a local key; however, if this string has not been used 
		// as a local key before, it may not be 4-byte aligned, and
		// therefore we need to adjust the key handle
		// LOG("Encoded key handle #%d", keyHandle);
		(pTile_+ofs_).putIntUnaligned(((keyHandle - originOfs_) << 1) | valueFlags);
		hasher_.addKey(key);
	}

	void writeGlobalKey(int valueFlags, uint32_t keyCode)
	{
		assert(keyCode <= FeatureConstants::MAX_COMMON_KEY);
		prevKeyOfs_ = ofs_;
		(pTile_ + ofs_).putUnsignedShort(
			static_cast<uint16_t>((keyCode << 2) | valueFlags));
		ofs_ += 2;
		hasher_.addKey(keyCode);
	}


	void writeStringValue(TString* value)
	{
		(pTile_ + ofs_).putIntUnaligned(value->handle() - ofs_);
		hasher_.addValue(value);
	}

	MutableDataPtr pTile_;
	int_fast32_t ofs_;
	int_fast32_t prevKeyOfs_;
	int_fast32_t tableOfs_;
	int_fast32_t originOfs_;
	TagTableHasher hasher_;
};
