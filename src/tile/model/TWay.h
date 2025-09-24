// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/util/log.h>
#include "TFeature2D.h"
#include <clarisma/util/Pointers.h>

class TWay;

// needsFixup flag is set if any local nodes are present

class TWayBody : public TFeatureBody
{
public:
	TWayBody(TilePtr base, DataPtr data, uint32_t size, uint32_t anchor) :
		TFeatureBody(base, Type::WAY_BODY, data, size, anchor ? Alignment::WORD : Alignment::BYTE, anchor)
	{
	}

	TWayBody() : TFeatureBody(Type::WAY_BODY) {}

	DataPtr nodeTable()
	{
		const TFeature* way = feature();
		int flags = way->feature().flags();
		return (flags & FeatureFlags::WAYNODE) ?
			DataPtr(data() - (flags & FeatureFlags::RELATION_MEMBER)) : DataPtr();
	}

	int nodeCount() const;
	void write(const TileModel& tile) const;
};


class TWay : public TFeature2D
{
public:
	static constexpr FeatureType FEATURE_TYPE = FeatureType::WAY;
	using STRUCT = SFeature;

	TWay(TilePtr base, WayPtr way, DataPtr pBodyData, uint32_t bodySize, uint32_t bodyAnchor) :
		TFeature2D(Pointers::delta32(way.ptr(), base.ptr()), way),
		body_(base, pBodyData, bodySize, bodyAnchor)
	{
	}

	TWay(TElement::Handle handle, FeaturePtr way) : TFeature2D(handle, way) {}
	
	TWayBody* body()
	{
		assert(&body_ == TFeature2D::body());
		return &body_;
	}

	void placeBody(Layout& layout);

public:
	TWayBody body_;
};


static_assert(offsetof(TWay, body_) == sizeof(TFeature2D),
	"Unexpected class layout");

