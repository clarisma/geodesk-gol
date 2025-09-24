// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TDataElement.h"

/**
 * A TElement that can be indexed by its Handle 
 * (TFeature, TString, TTagTable, TRelationTable)
 */
class TReferencedElement : public TDataElement
{
public:
	TReferencedElement(Type type, Handle handle, DataPtr data, uint32_t size,
		Alignment alignment, int anchor) :
		TDataElement(type, handle, data, size, alignment, anchor),
		nextByHandle_(nullptr)
	{
	}

private:
	TReferencedElement* nextByHandle_;

	friend class LookupByHandle;
};


class LookupByHandle : public Lookup<LookupByHandle, TReferencedElement>
{
public:
	static uint64_t getId(TReferencedElement* element)
	{
		return element->handle();
	}

	static TReferencedElement** next(TReferencedElement* elem)
	{
		return &elem->nextByHandle_;
	}
};

