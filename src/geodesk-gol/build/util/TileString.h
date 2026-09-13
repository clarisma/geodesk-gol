// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <cassert>
#include <clarisma/util/Strings.h>


/**
 * An identifier for a string used in a Tile. This can be a global-string code,
 * a number, 
 *
 * The encoding has the following format:
 *
 *	If string is shared:
 *		Bit 0-1		number of varint bytes (-1)
 *		Bit 2-31	encoded varint28 that represents the shared-string code
 *					(Its bit 0 -- hence bit 2 in this value -- is always 1)
 *  If string is literal:
 *      Bit 0-2     always 0
 *		Bit 3-31	offset to ShortVarString within a memory section
 */
class ProtoString
{
public:
	ProtoString() : data_(0) {}

	ProtoString(uint32_t sharedNumber)
	{
		uint32_t encoded;
		uint8_t* start = reinterpret_cast<uint8_t*>(&encoded);
		uint8_t* p = start;
		writeVarint(p, (sharedNumber << 1) | 1);
		data_ = (encoded << 2) | ((p - start) - 1);
	}

	ProtoString(const ShortVarString* str, const uint8_t* stringBase)
	{
		ptrdiff_t ofs = reinterpret_cast<const uint8_t*>(str) - stringBase;
		assert(ofs > 0);
		assert(ofs < (1 << 29));	// offset has only 29 bits
		data_ = ofs << 3;
	}

	bool isNull() const { return data_ == 0; }

	void writeTo(BufferWriter& out, const uint8_t* stringBase)
	{
		if (data_ & SHARED_STRING_FLAG)
		{
			// write the varint-encoded proto-string code (including the marker bit)
			uint32_t byteCount = (data_ & 3) + 1;
			uint32_t varints = data_ >> 2;
			out.writeBytes(reinterpret_cast<const uint8_t*>(&varints), byteCount);
		}
		else
		{
			uint32_t ofs = data_ >> 3;
			const uint8_t* bytes = stringBase + ofs;
			uint32_t len = *bytes;
			if (len & 0x80)
			{
				bytes++;
				len = (len & 0x7f) | (*bytes << 7);
			}
			uint32_t encodedLen = len << 1;
			if (encodedLen > 0x7f)
			{
				out.writeByte((encodedLen & 0x7f) | 0x80);
				out.writeByte(encodedLen >> 7);
				// TODO: We are encoding the string length as a varint13, since
				// we're using Bit 0 as the shared-vs-literal disciminator 
				// This limits string length to ~8K instead of ~16K
			}
			else
			{
				out.writeByte(encodedLen);
			}
			out.writeBytes(bytes + 1, len);
		}
	}

private:
	static const uint32_t SHARED_STRING_FLAG = 4;

	uint32_t data_;
};


class ProtoStringPair
{
public:
	enum
	{
		KEY = 0,
		VALUE = 1
	};

	ProtoStringPair() {}

	ProtoStringPair(ProtoString key, ProtoString value) :
		strings_{ key, value }
	{
	}

	ProtoString get(int type) const noexcept
	{
		assert(type == KEY || type == VALUE);
		return strings_[type];
	}

	void set(int type, ProtoString str) noexcept
	{
		assert(type == KEY || type == VALUE);
		strings_[type] = str;
	}

	ProtoString key() const noexcept
	{
		return strings_[KEY];
	}

	ProtoString value() const noexcept
	{
		return strings_[VALUE];
	}

	ProtoString strings_[2];
};