// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <string.h>
#include <clarisma/data/Linked.h>
#include <clarisma/data/Span.h>
#include "SortedChildFeature.h"

using namespace clarisma;
using namespace geodesk;

class SuperRelation : public Linked<SuperRelation>
{
public:
	SuperRelation(uint64_t id, TilePair tentativeTilePair, 
		Span<SortedChildFeature> members, Span<uint8_t> body,
		int missingNodesAndWays) :
		Linked(nullptr),
		id_(id),
		isResolved_(false),
		isPending_(false),
		highestMemberZoom_(-1),
		level_(0),
		tilePair_(tentativeTilePair),
		pilePair_(0),
		missingMemberCount_(missingNodesAndWays),
		removedRefcyleCount_(0),
		members_(members),
		body_(body)
	{
	}

	uint64_t id() const { return id_; }
	TilePair tilePair() const { return tilePair_; }
	int pilePair() const { return pilePair_; }
	int highestMemberZoom() const { return highestMemberZoom_; }
	int missingMemberCount() const { return missingMemberCount_; }
	int removedRefcyleCount() const { return removedRefcyleCount_; }

	Span<SortedChildFeature> members() const
	{
		return members_;
	}

	ByteSpan body() const
	{
		return body_;
	}

	/*
	int countNodesAndWays() const
	{
		int count = 0;
		for (SortedChildFeature member : members_)
		{
			if ((member.typedId & 3) != 2) count++;
		}
		return count;
	}
	*/

	// Clears the first occurrence of the given member
	void clearMember(uint64_t typedId)
	{
		for (SortedChildFeature& member : members_)
		{
			if (member.typedId == typedId)
			{
				member.typedId = 0;
				return;
			}
		}
	}

private:
	void validate();
	void recode(int removedRelationCount);

	uint64_t id_;
	TilePair tilePair_;
	int pilePair_;

	// The relation and any child relations has been processed; however, its
	// tilePair_ may be null, which means all of its members are missing or 
	// have been omitted, which means it should be omitted itself
	bool isResolved_;

	// The relation is in the process of being resolved 
	// (used to detect a refcycle)
	bool isPending_;

	int8_t highestMemberZoom_;
	
	// The super-relation level: 
	// 0 = it has no other relations (all of its member relations are missing, 
	//     or have been removed because the relation is at the bottom of a cycle)
	// 1 = it has child relations, but none of them have child relations themselves  
	// 2 = it has child relations, which are at most level 1
	// 3 = etc.
	int level_;

	// The total number of members that are missing or have been omitted
	int missingMemberCount_;

	// The number of refcycles from which this relation has been removed
	int removedRefcyleCount_;
	Span<SortedChildFeature> members_;
	Span<uint8_t> body_;

	friend class SuperRelationResolver;
};



