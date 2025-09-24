// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <cassert>
#include <clarisma/util/BufferWriter.h>
#include <clarisma/util/Pointers.h>
#include <clarisma/util/Strings.h>

using namespace clarisma;

// TODO: Should we place roles with keys instead of values, as they
//  have similar restrictions as keys? (number must be represented as string)

/**
 * A representation of a string used by the Sorter, Validator and Compiler.
 * A ProtoString may be represented as a shared-string code (if the string
 * occurs frequently enough) or as an offset to a literal string.
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
		union
		{
			uint8_t buf[4];
			uint32_t encoded;
		};
		encoded = 0;			// TODO: technically not needed
		uint8_t* p = buf;
		writeVarint(p, (sharedNumber << 1) | 1);
		assert(p - buf > 0);
		assert(p - buf <= 4);
		data_ = (encoded << 2) | ((p - buf) - 1);
	}

	ProtoString(const ShortVarString* str, const uint8_t* stringBase)
	{
		uint32_t ofs = Pointers::offset32(str, stringBase);
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
			union
			{
				uint8_t buf[4];
				uint32_t varints;
			};
			varints = data_ >> 2;
			out.writeBytes(buf, byteCount);
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