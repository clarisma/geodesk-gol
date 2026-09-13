// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TElement.h"

class FeatureTable;
class HilbertIndexBuilder;
class IndexSettings;
class Layout;
class TileModel;
class TFeature;
class TIndexTrunk;


class TIndex : public TElement
{
public:
	TIndex();
	void addFeature(TFeature* feature, int category, uint32_t indexBits)
	{
		assert(category >= 0 && category < NUMBER_OF_ROOTS);
		roots_[category].addFeature(feature, indexBits);
	}

	bool isEmpty() const { return rootCount_ == 0; }
	void build(TileModel& tile, const IndexSettings& settings);
	void place(Layout& layout);
	void write(const TileModel& tile) const;

	static constexpr int MAX_CATEGORIES = 30;
	static constexpr int NUMBER_OF_ROOTS = MAX_CATEGORIES + 2;
		// includes no-category (first) and multi-category (last)
	static constexpr int MULTI_CATEGORY = NUMBER_OF_ROOTS - 1;
	static constexpr int UNASSIGNED_CATEGORY = 255;

private:

	struct Root
	{
		int32_t indexBits;
		uint32_t featureCount;
		union
		{
			TIndexTrunk* trunk;
			TFeature* firstFeature;
		};

		bool isEmpty() const { return featureCount == 0; }
		void addFeature(TFeature* feature, uint32_t indexBits);
		void add(Root& other);
		void build(HilbertIndexBuilder& rtreeBuilder);
	};

	int getFeatureCategory(TFeature* feature);

	Root roots_[NUMBER_OF_ROOTS];
	int8_t next_[NUMBER_OF_ROOTS];
	int8_t firstRoot_;
	int rootCount_;
};

/*
class Indexer
{
public:
	Indexer(TileModel& tile, const IndexSettings& settings);
	void addFeatures(const FeatureTable& features);
	void build();
	void place(Layout& layout);

private:
	static const uint8_t FLAGS_TO_TYPE[16];

	enum
	{
		NODES,
		WAYS,
		AREAS,
		RELATIONS,
		INVALID
	};

	TileModel& tile_;
	const IndexSettings& settings_;
	TIndex indexes_[4];			// for nodes, ways, areas & relations
};
*/