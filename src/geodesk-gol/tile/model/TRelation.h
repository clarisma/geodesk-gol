// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TFeature2D.h"
#include <clarisma/util/Pointers.h>

class TRelation;

// needsFixup flag is set if any local members or local role strings are present

class TRelationBody : public TFeatureBody
{
public:
	TRelationBody(TilePtr base, DataPtr data, uint32_t size, int anchor) :
		TFeatureBody(base, Type::RELATION_BODY, data, size, Alignment::WORD, anchor)
	{
	}

	TRelationBody() : TFeatureBody(Type::RELATION_BODY)	{}

	void write(const TileModel& tile) const;
};

class TRelation : public TFeature2D
{
public:
	static constexpr FeatureType FEATURE_TYPE = FeatureType::RELATION;
	using STRUCT = SFeature;

	TRelation(TilePtr base, RelationPtr rel, DataPtr pBodyData, uint32_t bodySize) :
		TFeature2D(Pointers::delta32(rel.ptr(), base.ptr()), rel),
		body_(base, pBodyData, bodySize, rel.flags() & 4)
	{
	}

	TRelation(Handle handle, FeaturePtr rel) : TFeature2D(handle, rel) {}

	TRelationBody* body()
	{
		assert (&body_ == TFeature2D::body());
		return &body_;
	}


	void placeBody(Layout& layout);

public:
	TRelationBody body_;
};

static_assert(offsetof(TRelation, body_) == sizeof(TFeature2D),
	"Unexpected class layout");


