// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <unordered_map>
#include "SuperRelation.h"

class FastFeatureIndex;
class StringCatalog;
class TileCatalog;

class SuperRelationResolver
{
public:
	SuperRelationResolver(
		int estimatedCount,
		const TileCatalog& tileCatalog,
		const StringCatalog& strings,
		FastFeatureIndex& relationIndex) :
		tileCatalog_(tileCatalog),
		strings_(strings),
		relationIndex_(relationIndex)
	{
		superRelationsById_.reserve(estimatedCount);
	}

	void add(SuperRelation* rel)
	{
		superRelations_.addTail(rel);
		superRelationsById_[rel->id()] = rel;
	}

	const std::vector<SuperRelation*>* resolve();

	static constexpr int MAX_RELATION_LEVEL = 9;

private:
	struct CyclicalRelation
	{
		CyclicalRelation(SuperRelation* r, SuperRelation* c) :
			score(0), relation(r), child(c) {}

		double score;
		SuperRelation* relation;
		SuperRelation* child;

		// Sorted by default from lowest to highest score
		bool operator<(const CyclicalRelation& other) const noexcept
		{
			return score < other.score;
			// TODO: break tie based on ID
		}
	};

	bool resolve(SuperRelation* rel);
	SuperRelation* breakReferenceCycle();
	double calculateScore(const SuperRelation* rel) const;

	LinkedQueue<SuperRelation> superRelations_;
	std::unordered_map<uint64_t, SuperRelation*> superRelationsById_;
	const TileCatalog& tileCatalog_;
	const StringCatalog& strings_;
	FastFeatureIndex& relationIndex_;
	std::vector<CyclicalRelation> cyclicalRelations_;
	std::vector<SuperRelation*> levels_[MAX_RELATION_LEVEL+1];
};