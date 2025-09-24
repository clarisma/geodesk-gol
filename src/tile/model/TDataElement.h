// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TElement.h"
#include <clarisma/util/MutableDataPtr.h>
#include <geodesk/feature/TilePtr.h>

/**
 * A TElement that has a pointer to data (original or modified)
 */
class TDataElement : public TElement
{
public:
	TDataElement(Type type, Handle handle, DataPtr data, uint32_t size,
		Alignment alignment, int anchor) :
		TElement(type, handle, size, alignment, anchor),
		data_(data)
	{
	}

	const DataPtr data() const { return data_; }
	const DataPtr dataStart() const { return data_ - anchor(); }
	// MutableDataPtr mutableData() const { return MutableDataPtr(data_); }
	void setData(DataPtr data) { data_ = data; }
	TilePtr base() const 
	{ 
		return TilePtr(
			reinterpret_cast<const uint8_t*>(
			(data() - handle()) & 0xffff'ffff'ffff'fffcULL));
	}

protected:
	DataPtr data_;
};
