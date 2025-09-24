// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <geodesk/geom/Box.h>

struct VForeignFeature2D;
struct VLocalNode;
struct VLocalFeature2D;
struct VNode;

struct VFeature
{
	enum Flags
	{
		// The feature is foreign
		FOREIGN = 1 << 0,

		// Indicates that the bounds and exports of a way or relation
		// have been calculated; pData has been replaced by pBounds
		PROCESSED = 1 << 1,

		// The node has tags
		TAGGED_NODE = 1 << 2,

		// The relation needs to be exported because it has members
		// that live at higher zoom levels
		EXPORT_RELATION_ALWAYS = 1 << 2,

		// The node has the same coordinates as at least one other node
		NODE_SHARES_LOCATION = 1 << 3,

		// The node belongs to at least one way
		WAY_NODE = 1 << 4,

		// The node belongs to at least one relation
		RELATION_NODE = 1 << 5,

	};

	VFeature(uint64_t typedId, int flags) :
		idAndFlags((typedId << 6) | flags), next(nullptr) {}
	
	int type() const {	return static_cast<int>(idAndFlags >> 6) & 3; }
	uint64_t id() const { return idAndFlags >> 8; }
	TypedFeatureId typedId() const { return TypedFeatureId(idAndFlags >> 6); }
	bool isNode() const { return type() == 0; }
	bool isWay() const { return type() == 1; }
	bool isRelation() const { return type() == 2; }
	bool isForeign() const { return (idAndFlags & Flags::FOREIGN) != 0;	}
	bool isProcessed() const { return (idAndFlags & Flags::PROCESSED) != 0; }
	void setFlag(int flag) { idAndFlags |= flag; }
	
	
	VNode* asNode()
	{
		assert(isNode());
		return reinterpret_cast<VNode*>(this);
	}

	VLocalNode* asLocalNode() 
	{
		assert(isNode() && !isForeign());
		return reinterpret_cast<VLocalNode*>(this);
	}

	VLocalFeature2D* asLocalFeature2D()
	{
		assert(!isNode() && !isForeign());
		return reinterpret_cast<VLocalFeature2D*>(this);
	}

	VForeignFeature2D* asForeignFeature2D()
	{
		assert(!isNode() && isForeign());
		return reinterpret_cast<VForeignFeature2D*>(this);
	}

	uint64_t idAndFlags;
		// Bit 0		1 = foreign
		// Bit 1		1 = processed	
		// Bit 2		1 = tagged_node
		//              1 = export_relation_always (relations only)
		// Bit 3		1 = node_shares_location (nodes only)
		// Bit 4		1 = way_node (nodes only)
		// Bit 5		1 = relation_node (nodes only)
		// Bit 3-5		twin_code (ways and relations only)
		// Bit 6-7		type (0=node, 1=way, 2=relation)
		// Bit 8-31		ID
	union
	{
		VFeature* next;
		int tex;
	};
};

struct VNode : VFeature
{
	VNode(uint64_t id, int flags, Coordinate xy_) :
		VFeature(id << 2, flags), xy(xy_) {}
	
	Coordinate xy;
};

struct VLocalNode : VNode
{
	VLocalNode(uint64_t id, int flags, Coordinate xy_) :
		VNode(id, flags, xy_), tiles(0) {}

	bool isExported() const
	{
		return tiles != 0;
	}

	bool isFeatureNode() const
	{
		return (idAndFlags &
			(RELATION_NODE | TAGGED_NODE | NODE_SHARES_LOCATION)) != 0;
	}

	bool hasTags() const
	{
		return (idAndFlags & TAGGED_NODE) != 0;
	}

	bool isSpecial() const
	{
		return hasSharedLocation() || isOrphan();
	}

	bool hasSharedLocation() const
	{
		return (idAndFlags & NODE_SHARES_LOCATION) != 0;
	}

	bool isOrphan() const
	{
		return (idAndFlags & (WAY_NODE | RELATION_NODE | TAGGED_NODE)) == 0;
	}

	bool isRelationMember() const
	{
		return (idAndFlags & RELATION_NODE) != 0;
	}

	uint64_t tiles;
};

struct VLocalBounds
{
	Box bounds;
	uint64_t tiles;

	explicit VLocalBounds(uint64_t tentativeTiles) : tiles(tentativeTiles) {}
};

struct VLocalFeature2D : VFeature
{
	union
	{
		VLocalBounds* bounds;
		uint64_t tentativeTiles;
	};
	const uint8_t* body;

	VLocalFeature2D(FeatureType type, uint64_t id, ParentTileLocator locator, const uint8_t* body_) :
		VFeature((id << 2) | static_cast<int>(type), locator.twinCode() << 3), bounds(nullptr), body(body_) {}

	bool isRelationAlwaysExported() const
	{
		assert(isRelation());
		return idAndFlags & Flags::EXPORT_RELATION_ALWAYS;
	}

	int twinCode() const
	{
		return static_cast<int>(idAndFlags >> 3) & 7;
	}
};

struct VForeignFeature2D : VFeature
{
	VForeignFeature2D(FeatureType type, uint64_t id, const Box& b) :
		VFeature((id << 2) | static_cast<int>(type), Flags::FOREIGN), bounds(b) {}

	Box bounds;
};

static_assert(sizeof(VLocalNode) == 32, "Size of local features must be 32");
static_assert(sizeof(VLocalFeature2D) == 32, "Size of local features must be 32");
