// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/alloc/Block.h>
#include "build/util/ForeignRelationLookup.h"
#include <geodesk/feature/TypedFeatureId.h>

class ExportTable : public ByteBlock
{
public:
	struct Header
	{
		uint32_t exportedRelationsCount;
		uint32_t exportsCount;
	};

	ExportTable(size_t exportsCount, size_t exportedRelationsCount) :
		Block(sizeof(Header) + sizeof(TypedFeatureId) * exportsCount +
			sizeof(ForeignRelationLookup::Entry) * exportedRelationsCount)
	{
		Header* h = header();
		h->exportsCount = static_cast<uint32_t>(exportsCount);
		h->exportedRelationsCount = static_cast<uint32_t>(exportedRelationsCount);
	}

	Header* header()
	{
		return reinterpret_cast<Header*>(data());
	}

	Span<ForeignRelationLookup::Entry> exportedRelations() 
	{
		return Span<ForeignRelationLookup::Entry>(
			reinterpret_cast<ForeignRelationLookup::Entry*>(data() + sizeof(Header)),
				header()->exportedRelationsCount);
	}

	Span<TypedFeatureId> exports() 
	{
		Header* h = header();
		return Span<TypedFeatureId>(
			reinterpret_cast<TypedFeatureId*>(data() + sizeof(Header) +
				sizeof(ForeignRelationLookup::Entry) * h->exportedRelationsCount),
					h->exportsCount);
	}
};
