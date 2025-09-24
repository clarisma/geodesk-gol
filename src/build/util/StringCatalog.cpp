// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "StringCatalog.h"
#include <algorithm>
#include <clarisma/cli/Console.h>
#include <clarisma/math/Decimal.h>
#include "BuildSettings.h"
#include "StringStatistics.h"
#include <geodesk/feature/TagValues.h>
#include <geodesk/feature/types.h>

// TODO: Don't place numbers into the GST (at least not narrow numbers)

StringCatalog::StringCatalog() :
	table_(nullptr),
	tableSlotCount_(0),
	globalStringCount_(0),
	globalStringDataSize_(0)
{

}

// Must match the constants in StringTable::Constant
const char* StringCatalog::CORE_STRINGS[] =
{
	"",
	"no",
	"yes",
	"outer",
	"inner"
};


/**
 * - Step 1: Count strings and measure required space
 * - Step 2
 *   - Build the string -> proto-code lookup, but don't fill it yet
 *   - Build 3 sort tables (pointer to lookup entry, total usage)
 *     - keys, values, combined
 *   - Sort the tables
 * - Step 3
 *   - Build the GST
 *     - add the core strings, mark them in the proto-code lookup
 *     - add indexed keys, mark them in the proto-code lookup
 *     - add most-frequent strings, up to pos. 255, mark them
 *     - add most-frequent keys, up to pos. 8191, mark them
 *     - add rest of strings
 *   - Step 4
 *     
 */

// TODO: Check the maximum possible strings in the ProtoString table
//  and enforce this limit (may already be enforced by virtue of the
//  string arena size)

void StringCatalog::build(const BuildSettings& settings, ByteSpan strings)
{
	// The minimum number of times a string must be used in order
	// to be included in the proto-string table
	uint32_t minProtoStringUsage = 100;

	// The minimum number of times a string must be used for keys or values
	// in order to be assigned a code in the proto-string table
	uint32_t minKeyValueProtoStringUsage = minProtoStringUsage / 2;

	uint32_t protoStringCount = 0;
	uint32_t protoKeyCount = 0;
	uint32_t protoValueCount = 0;
	uint32_t totalEntrySizeInBytes = 0;
	StringStatistics::Iterator iter(strings);

	// First, count the number of strings that should be placed in the
	// proto-string table and measure the total space required to store them

	for (;;)
	{
		const StringStatistics::Counter* counter = iter.next();
		if (!counter) break;
		if (counter->totalCount() < minProtoStringUsage) continue;

			// The ProtoString table must include "required" strings
			// totalCount() / keyCount() / valueCount() include
			// the REQUIRED flag to ensure that these counters are
			// always above any minimum

		protoStringCount++;
		uint32_t stringSize = counter->stringSize();
		totalEntrySizeInBytes += Entry::totalSize(stringSize);
	}
	if(Console::verbosity() >= Console::Verbosity::DEBUG)	[[unlikely]]
	{
		Console::msg("Proto-string table has %d strings (%d total bytes)",
			protoStringCount, totalEntrySizeInBytes);
	}

	// Allocate space for the proto-string table and copy the strings
	// into it, but don't create the actual lookup table yet
	// At the same time, create three temporary tables used to sort
	// pointers to the table entries based on the occurrence count 

	uint32_t tableSlotCount = Bytes::roundUpToPowerOf2(protoStringCount * 2);
	uint32_t tableSizeInBytes = sizeof(uint32_t) * tableSlotCount;
	uint32_t arenaSizeInBytes = tableSizeInBytes + totalEntrySizeInBytes;
	arena_.reset(new uint8_t[arenaSizeInBytes]);
	memset(arena_.get(), 0, arenaSizeInBytes);
		// TODO: probably better to use in-place news for each Entry
		// (but will also need to zero-fill the lookup table)

	std::vector<SortEntry> sorted;
	sorted.reserve(protoStringCount);
	std::vector<SortEntry> sortedKeys;
	sortedKeys.reserve(protoStringCount);
	std::vector<SortEntry> sortedValues;
	sortedValues.reserve(protoStringCount);

	Entry* pEntry = reinterpret_cast<Entry*>(arena_.get() + tableSizeInBytes);
	iter = StringStatistics::Iterator(strings);
	for (;;)
	{
		const StringStatistics::Counter* counter = iter.next();
		if (!counter) break;
		if (counter->totalCount() < minProtoStringUsage) continue;

		pEntry->string.init(&counter->string());
		pEntry->globalCodePlusOne = 0;
		sorted.emplace_back(counter->trueTotalCount(), pEntry);
		uint64_t keyCount = counter->keyCount();
		if (keyCount >= minKeyValueProtoStringUsage)
		{
			sortedKeys.emplace_back(keyCount, pEntry);
		}
		uint64_t valueCount = counter->valueCount();
		if (valueCount >= minKeyValueProtoStringUsage)
		{
			sortedValues.emplace_back(keyCount, pEntry);
		}
		pEntry = reinterpret_cast<Entry*>(
			reinterpret_cast<uint8_t*>(pEntry) + pEntry->totalSize());
	}

	// All strings that were counted in the first pass must be included
	// in the general sort table; for keys and values, there may be 
	// fewer strings
	assert(sorted.size() == protoStringCount);
	assert(sortedKeys.size() <= protoStringCount);
	assert(sortedValues.size() <= protoStringCount);

	sortDescending(sorted);
	sortDescending(sortedKeys);
	sortDescending(sortedValues);

	if(Console::verbosity() >= Console::Verbosity::DEBUG)	[[unlikely]]
	{
		Console::msg("Sorted strings in order of occurrence count.");
	}

	// Now, create the lookup table
	// We work backwards so we index the least-used strings first; in the
	// event of a hash collision, the more frequently used string will then
	// be placed towards the head of the linked list

	// memset(arena_.get(), 0, tableSizeInBytes);
	// (Not needed, we already zero-filled the entire arena)

	uint32_t* table = reinterpret_cast<uint32_t*>(arena_.get());
	for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) 
	{
		Entry* pEntry = it->second;
		uint32_t hash = Strings::hash(pEntry->string);
		uint32_t slot = hash % tableSlotCount;
		pEntry->next = table[slot];
		table[slot] = reinterpret_cast<uint8_t*>(pEntry) - arena_.get();
		// printf("%d: Indexed %s\n", slot, std::string(pEntry->string.toStringView()).c_str());
	}
	table_ = table;
	tableSlotCount_ = tableSlotCount;

	// Now we can build the global string table
	const std::vector<IndexedKey>& indexedKeys = settings.indexedKeys();
	int minGlobalStringCount = CORE_STRING_COUNT + indexedKeys.size();
	int maxGlobalStringCount = std::max(settings.maxStrings(), minGlobalStringCount);

	globalStrings_.reset(new uint32_t[maxGlobalStringCount]);
	for (int i = 0; i < StringCatalog::CORE_STRING_COUNT; i++)
	{
		addGlobalString(std::string_view(CORE_STRINGS[i]));
	}
	for (const IndexedKey& indexedKey : indexedKeys)
	{
		addGlobalString(indexedKey.key);
	}
	assert(globalStringCount_ <= maxGlobalStringCount);

	uint32_t minGlobalStringUsage = settings.minStringUsage();
	const static int MAX_MIXED_STRINGS = 512;
	int maxMixedStringCount = std::min(MAX_MIXED_STRINGS, maxGlobalStringCount);

	// Add the most common keys and values

	auto itSorted = sorted.begin();
	while (itSorted != sorted.end())
	{
		if (globalStringCount_ >= maxMixedStringCount) break;
		if (itSorted->first >= minGlobalStringUsage)
		{
			addGlobalString(itSorted->second);
		}
		itSorted++;
	}
	
	// Fill the table space up to 8K only with keys

	int maxKeyCount = std::min(1 << 13, maxGlobalStringCount);
		// TODO: use constant (max 8K keys);
	for (SortEntry entry : sortedKeys)
	{
		if (globalStringCount_ >= maxKeyCount) break;
		if (entry.first >= minGlobalStringUsage)
		{
			addGlobalString(entry.second);
		}
	}

	// Finally, add all remaining keys/values that fit into the table

	while (itSorted != sorted.end())
	{
		if (globalStringCount_ >= maxGlobalStringCount) break;
		if (itSorted->first >= minGlobalStringUsage)
		{
			addGlobalString(itSorted->second);
		}
		itSorted++;
	}

	if(Console::verbosity() >= Console::Verbosity::DEBUG)	[[unlikely]]
	{
		Console::msg("Created global string table with %d strings (%d bytes)",
			globalStringCount_, globalStringDataSize_);
	}

	createProtoStringCodes(sortedKeys, ProtoStringPair::KEY, FeatureConstants::MAX_COMMON_KEY);
	createProtoStringCodes(sortedValues, ProtoStringPair::VALUE, 0xffff);

}

void StringCatalog::sortDescending(std::vector<SortEntry>& sorted)
{
	std::sort(sorted.begin(), sorted.end(), [](const SortEntry& a, const SortEntry& b)
		{
			return a.first > b.first;
		});
}


StringCatalog::Entry* StringCatalog::lookup(const std::string_view str) const noexcept
{
	uint32_t slot = Strings::hash(str) % tableSlotCount_;
	// printf("Looking up '%s' in slot %d...\n", std::string(str).c_str(), slot);
	uint32_t ofs = table_[slot];
	while (ofs)
	{
		Entry* p = reinterpret_cast<Entry*>(arena_.get() + ofs);
		if (p->string == str) return p;
		ofs = p->next;
	}
	return nullptr;
}

void StringCatalog::addGlobalString(Entry* p)
{
	if (p->globalCodePlusOne == 0)
	{
		// String is not already in the GST

		// Narrow numbers are never stored in the GST, because they
		// can be encoded as numbers using 2 bytes
		// TODO: Decide whether wide numbers can be stored as global strings
		// (If so, can be encoded using 2 bytes instead of 4, at the cost
		// of having to parse the string -- this is the current approach)

		Decimal d(p->string.toStringView(), true);	
			// parse strictly (string must represent number in canonical form)
		if (!TagValues::isNarrowNumericValue(d))
		{
			globalStrings_[globalStringCount_] =
				reinterpret_cast<const uint8_t*>(&p->string) - stringBase();
			globalStringCount_++;
			globalStringDataSize_ += p->string.totalSize();
			p->globalCodePlusOne = globalStringCount_;
		}
	}
}

void StringCatalog::addGlobalString(std::string_view str)
{
	Entry* p = lookup(str);
	assert(p); // string must exist
	addGlobalString(p);
}

// TODO: Keep in mind that not all global string codes are suitable for keys & roles!
void StringCatalog::createProtoStringCodes(const std::vector<SortEntry>& sorted, 
	int type, int maxGlobalCode)
{
	std::unique_ptr<StringRef[]>& protoToRef = protoToRef_[type];
	protoToRef.reset(new StringRef[sorted.size()]);
	uint32_t pos = 0;
	for (SortEntry sortedEntry : sorted)
	{
		Entry* entry = sortedEntry.second;
		entry->protoStringPair.set(type, ProtoString(pos));
		protoToRef[pos] = entry->stringRef(stringBase(), maxGlobalCode);
		pos++;
	}
}

ProtoStringPair StringCatalog::protoStringPair(const ShortVarString* str, const uint8_t* stringBase) const
{
	Entry* p = lookup(str->toStringView());
	ProtoStringPair pair = p ? p->protoStringPair : ProtoStringPair();
	ProtoString literal(str, stringBase);
	return ProtoStringPair(
		!pair.key().isNull() ? pair.key() : literal,
		!pair.value().isNull() ? pair.value() : literal);
}


ByteBlock StringCatalog::createGlobalStringTable() const
{
	const size_t tableSize = globalStringDataSize_ + 2;
	uint8_t* data = new uint8_t[tableSize];
	uint8_t* p = data;
	*(reinterpret_cast<uint16_t*>(p)) = globalStringCount_;
	p += 2;
	for(int i=0; i<globalStringCount_; i++)
	{
		const ShortVarString* s = getGlobalString(i);
		size_t size = s->totalSize();
		memcpy(p, s, size);
		p += size;
	}
	assert(p == data + tableSize);
	return ByteBlock(data, tableSize);
}
