// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/util/Pointers.h>
#include <geodesk/feature/TilePtr.h>
#include "TFeature.h"

class TFeatureBody;

/**
 * Base class for TWay and TRelation, which have the following in common:
 * - Stub of size 32 with anchor 16
 * - a TFeatureBody
 * - If member of a relation, a pointer to a relation table that is located
 *   just ahead of the body's anchor
 */
class TFeature2D : public TFeature
{
public:
	TFeature2D(Handle handle, FeaturePtr feature) :
		TFeature(Type::FEATURE2D, handle, 32, feature, 16)
	{
	}

	TFeatureBody* body()	
	{
		return reinterpret_cast<TFeatureBody*>(
			reinterpret_cast<uint8_t*>(this) + sizeof(TFeature2D));
	}

	const TFeatureBody* constBody() const
	{
		return reinterpret_cast<const TFeatureBody*>(
			reinterpret_cast<const uint8_t*>(this) + sizeof(TFeature2D));
	}

	/**
	 * Returns a pointer to the relation table of this way or relation, 
	 * or `nullptr` if it is not a relation member.
	 */
	TRelationTable* parentRelations(TileModel& tile) const;
	void setParentRelations(TRelationTable* rels);

	void write(const TileModel& tile) const;
};


// TODO: We could store a pointer to build information as a union
//  if the body's next pointer, since it is not used until we place the body

class TFeatureBody : public TDataElement
{
public:
	TFeatureBody(TilePtr base, Type type, DataPtr data, 
		uint32_t size, Alignment alignment, uint32_t anchor) :
		TDataElement(type, Pointers::delta32(data, base.ptr()), data, size, alignment, anchor)
	{
	}

	explicit TFeatureBody(Type type) :
		TDataElement(type, 0, DataPtr(), 0, Alignment::WORD, 0)
	{
	}

	// TODO: When creating the actual WayBody, need to set alignment

	TFeature2D* feature() 
	{
		return reinterpret_cast<TFeature2D*>(
			reinterpret_cast<uint8_t*>(this) - sizeof(TFeature2D));
	}

	const TFeature2D* constFeature() const
	{
		return reinterpret_cast<const TFeature2D*>(
			reinterpret_cast<const uint8_t*>(this) - sizeof(TFeature2D));
	}

protected:
	void fixRelationTablePtr(uint8_t* pBodyStart, const TileModel& tile) const;
};


