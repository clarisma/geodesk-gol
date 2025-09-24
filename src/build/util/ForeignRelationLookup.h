// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once


#include <clarisma/alloc/Block.h>
#include <clarisma/data/CompactHashTable.h>
#include <geodesk/feature/Tex.h>



struct ForeignRelationLookupEntry
{
	ForeignRelationLookupEntry(uint64_t id_, Tex tex_) :
		id(id_), tex(tex_), next(0) {}
	ForeignRelationLookupEntry() : id(0), tex(0), next(0) {}

	uint64_t id;
	Tex tex;
	uint32_t next;
};

class ForeignRelationLookup : public CompactHashTable<ForeignRelationLookup, ForeignRelationLookupEntry,uint64_t>
{
public:
	using Entry = ForeignRelationLookupEntry;

	static uint64_t key(const Entry& item)
	{
		return item.id;
	}

	static uint32_t next(const Entry& item)
	{
		return item.next;
	}

	static void setNext(Entry& item, uint32_t next)
	{
		item.next = next;
	}

private:
	Block<Entry> table_;
};

struct ForeignRelationTable
{
	const size_t size;
	const ForeignRelationLookup::Entry entries[1];		// variable size;

	Span<const ForeignRelationLookup::Entry> asSpan() const
	{
		return Span<const ForeignRelationLookup::Entry>(entries, size);
	}
};
