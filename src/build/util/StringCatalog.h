// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <memory>
#include <vector>
#include <clarisma/util/Bytes.h>
#include "ProtoString.h"

using namespace clarisma;

class BuildSettings;
class StringStatistics;


/**
 * The StringCatalog maintains the various string lookup tables.
 * 
 * - A table that translates a literal string to a pair of encoded ProtoString 
 *   codes (for keys and values)
 * 
 * - A table that translates a ProtoString code to the global-string code or
 *   literal string (two entries: key and value)
 */
class StringCatalog
{
public:
	StringCatalog();

	class StringRef
	{
	public:
		StringRef() : data_(0) {}
		StringRef(uint32_t d) : data_(d) {}

		bool isNull() const noexcept { return data_ == 0; }
		bool isGlobalCode() const noexcept { return data_ & 1; }
		uint32_t globalCode() const noexcept
		{
			assert(isGlobalCode());
			return data_ >> 1;
		}

		uint32_t stringOfs() const noexcept
		{
			assert(!isGlobalCode());
			return data_ >> 1;
		}

	private:
		uint32_t data_;
	};

	void build(const BuildSettings& settings, ByteSpan strings);
	ProtoStringPair protoStringPair(const ShortVarString* str, const uint8_t* stringBase) const;
	const uint8_t* stringBase() const noexcept { return arena_.get(); }
	ByteBlock createGlobalStringTable() const;

	const ShortVarString* getString(uint32_t ofs) const noexcept
	{
		return reinterpret_cast<const ShortVarString*>(stringBase() + ofs);
	}

	const ShortVarString* getString(StringRef ref) const noexcept
	{
		if (ref.isGlobalCode())
		{
			return getGlobalString(ref.globalCode());
		}
		else
		{
			return getString(ref.stringOfs());
		}
	}

	int getGlobalCode(std::string_view s) const noexcept
	{
		const Entry* entry = lookup(s);
		if (entry == nullptr) return -1;
		return static_cast<int>(entry->globalCodePlusOne) - 1;
	}
		
	const ShortVarString* getGlobalString(uint32_t code) const noexcept
	{
		assert(code < globalStringCount_);
		return getString(globalStrings_[code]);
	}

	StringRef stringRef(int type, uint32_t protoStringCode) const noexcept
	{
		assert(type == ProtoStringPair::KEY || type == ProtoStringPair::VALUE);
		return protoToRef_[type][protoStringCode];
	}

	StringRef keyStringRef(uint32_t protoStringCode) const noexcept
	{
		return stringRef(ProtoStringPair::KEY, protoStringCode);
	}

	StringRef valueStringRef(uint32_t protoStringCode) const noexcept
	{
		return stringRef(ProtoStringPair::VALUE, protoStringCode);
	}
	
	static const char* CORE_STRINGS[];
	static const int CORE_STRING_COUNT = 5;

private:
	struct EntryHeader
	{
		uint32_t next;
		uint32_t globalCodePlusOne;
		ProtoStringPair protoStringPair;
	};

	struct Entry : public EntryHeader
	{
		ShortVarString string;

		static uint32_t totalSize(uint32_t stringSize)
		{
			return static_cast<uint32_t>(sizeof(EntryHeader) +
				Bytes::aligned(stringSize, alignof(Entry)));
		}

		uint32_t totalSize() const noexcept
		{
			return totalSize(string.totalSize());
		}

		uint32_t stringOfs(const uint8_t* stringBase) const noexcept
		{
			return static_cast<uint32_t>(
				reinterpret_cast<const uint8_t*>(&string) - stringBase);
		}

		StringRef stringRef(const uint8_t* stringBase, int maxGlobalCode) const noexcept
		{
			return StringRef(
				((globalCodePlusOne-1) <= static_cast<uint32_t>(maxGlobalCode)) ?
				(((globalCodePlusOne - 1) << 1) | 1) :
				(stringOfs(stringBase) << 1));

			// globalCodePlusOne is a uint32_t. If it is 0, by subtracting 1 we force
			// it to wrap to 0xffff'ffff, hence we only need one comparison
			// (maxGlobalCode is needed because if the StringRef is intended as a key
			// string, only global codes up to and including FeatureConstants::MAX_COMMON_KEY
			// are valid)
		}
	};

	using SortEntry = std::pair<uint64_t, Entry*>;

	Entry* getEntry(uint32_t strOfs)
	{
		return reinterpret_cast<Entry*>(arena_.get() + strOfs - offsetof(Entry, string));
	}
	static void sortDescending(std::vector<SortEntry>& sorted);
	Entry* lookup(const std::string_view str) const noexcept;
	void addGlobalString(Entry* p);
	void addGlobalString(std::string_view str);
	void createProtoStringCodes(const std::vector<SortEntry>& sorted, int type, int maxGlobalCode);

	std::unique_ptr<uint8_t[]> arena_;
	const uint32_t* table_;
	uint32_t tableSlotCount_;
	std::unique_ptr<uint32_t[]> globalStrings_;
	uint32_t globalStringCount_;
	uint32_t globalStringDataSize_;
	std::unique_ptr<StringRef[]> protoToRef_[2];
};
