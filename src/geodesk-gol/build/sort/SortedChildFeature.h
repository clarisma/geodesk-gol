// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <geodesk/geom/TilePair.h>

using namespace geodesk;

struct SortedChildFeature
{
	SortedChildFeature(uint64_t id_, int pile_, TilePair tilePair_) :
		id(id_),		// can be typed
		pile(pile_),	// can be a pair
		tilePair(tilePair_)
	{
	}

	// Don't use TypedFeatureId here, because we may run afoul of the
	// Common Initial Sequence Rule
	union
	{
		uint64_t id;
		uint64_t typedId;
	};
	union
	{
		int pile;
		int pilePair;
	};
	TilePair tilePair;
};
