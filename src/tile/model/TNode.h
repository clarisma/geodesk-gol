// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TFeature.h"

class TNode : public TFeature
{
public:
	static constexpr FeatureType FEATURE_TYPE = FeatureType::NODE;
	using STRUCT = SNode;

	TNode(Handle handle, FeaturePtr node) :
		TFeature(TElement::Type::NODE, handle, 20 + (node.flags() & 4), node, 8)		// Bit 2 = member flag
	{
	}

	NodePtr node() const noexcept { return NodePtr(feature()); }

	Coordinate xy() const noexcept 
	{
		return node().xy();
	};

	TRelationTable* parentRelations(const TileModel& tile) const;
	void setParentRelations(TRelationTable* rels);

	void placeBody(Layout& layout)
	{
		if (node().isRelationMember()) placeRelationTable(layout);
	}

	// TODO
	void update(TileModel& model, uint64_t id, int flags, Coordinate xy,
		TTagTable* tags, TRelationTable* rels);

	void write(const TileModel& tile) const;
};


// To create a Node, we need:
// ID/flags
// x/y
// handle to tags
// handle to rels (member flag must be set)

