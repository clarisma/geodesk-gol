// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <utility> // for std::pair
#include "tile/model/TileModel.h"

class TFeature;
class TIndexBranch;
class TIndexLeaf;
class TIndexTrunk;

class HilbertIndexBuilder
{
public:
	explicit HilbertIndexBuilder(TileModel& tile, int rtreeBucketSize) :
		arena_(tile.arena()),
		tileBounds_(tile.bounds()),
		rtreeBucketSize_(rtreeBucketSize)
	{
	}

	/**
	 * Builds a spatial index for a set of features. Note that
	 * features are in a CIRCULAR LIST, and an explicit count
	 * must be passed (which must match the number of features)
	 * 
	 * @param firstFeature a circular list of features
	 * @param the number of features
	 */
	TIndexTrunk* build(TFeature* firstFeature, int count);

private:
	struct HilbertItem
	{
		uint32_t distance;
		TFeature* feature;

		bool operator<(const HilbertItem& other) const
		{
			return distance < other.distance;
		}
	};

	TIndexLeaf* createLeaf(HilbertItem* pFirst, int count);
	TIndexTrunk* createTrunk(TIndexBranch** pFirst, int count);

	Arena& arena_;
	Box tileBounds_;
	const int rtreeBucketSize_;
};
