// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/alloc/ReusableBlock.h>
#include <clarisma/data/Span.h>
#include <clarisma/util/varint.h>
#include <geodesk/feature/ForeignFeatureRef.h>
#include <geodesk/feature/TypedFeatureId.h>
#include "ParentTileLocator.h"
#include "ProtoGol.h"

template<typename Derived>
class ProtoGolReader
{
public:
	ProtoGolReader() :
		data_(256 * 1024, 8)		// Size in 512 KB increments, allow 8 wasteful cycles
	{
	}

	// Overrides

	void node(uint64_t id, Coordinate xy, ByteSpan tags) {}
	void way(uint64_t id, ParentTileLocator locator, ByteSpan body) {}
	void relation(uint64_t id, ParentTileLocator locator, ByteSpan body) {}
	void membership(uint64_t relId, ParentTileLocator locator, TypedFeatureId typedMemberId) {}
	void foreignNode(uint64_t id, Coordinate xy, ForeignFeatureRef ref) {}
	void foreignFeature(int type, uint64_t id, const Box& bounds, ForeignFeatureRef ref) {}
		// TODO: Use TypedFeatureId instead
	void specialNode(uint64_t id, int specialNodeFlags) {}
	Tip pileToTip(int pileNumber) { return Tip(); }

	void readTile()
	{
		const uint8_t* p = data_.data();
		const uint8_t* pEnd = p + data_.size();

		while (p < pEnd)
		{
			int groupMarker = *p++;
			int groupType = groupMarker & 7;
			FeatureType featureType = static_cast<FeatureType>(groupMarker >> 3);
			if (groupType == ProtoGol::GroupType::LOCAL_GROUP)
			{
				if (featureType == FeatureType::NODE)
				{
					self().readNodes(p);
				}
				else if (featureType == FeatureType::WAY)
				{
					self().readWays(p);
				}
				else if (featureType == FeatureType::RELATION)
				{
					self().readRelations(p);
				}
				else
				{
					// TODO: Log.error("Unknown marker %d in tile %s (Pile %d)", groupMarker,
					//	Tile.toString(sourceTile), sourcePile);
					break;
				}
			}
			else if (groupType == ProtoGol::GroupType::EXPORTED_GROUP)
			{
				if (featureType == FeatureType::NODE)
				{
					self().readForeignNodes(p);
				}
				else
				{
					self().readForeignFeatures(featureType, p);
				}
			}
			else if (groupType == ProtoGol::GroupType::EXPORT_TABLE)
			{
				int count = readVarint32(p);
				self().readExportTable(count, p);
				assert(*p == 0);
				p++;
				// TODO: This is ugly; all groups expect an end marker
				//  so we have to write one for export tables as well,
				//  even though they don't need them (explicit length)
			}
			else
			{
				assert(groupType == ProtoGol::GroupType::SPECIAL_GROUP);
				self().readSpecialNodes(p);
			}
		}
	}

protected:
	void readExportTable(int count, const uint8_t*& p)
	{
		skipVarints(p, count);
	}

	void readNodes(const uint8_t*& p)
	{
		int64_t prevId = 0;
		int32_t x = 0;
		int32_t y = 0;
		for (;;)
		{
			int64_t id = readVarint64(p);
			if (id == 0) break;
			bool isTagged = (id & 1);
			id = prevId + (id >> 1);
			prevId = id;
			x += readSignedVarint32(p);
			y += readSignedVarint32(p);
			uint32_t tagsSize;
			if (isTagged)
			{
				tagsSize = readVarint32(p);
			}
			else
			{
				tagsSize = 0;
			}
			self().node(id, Coordinate(x,y), ByteSpan(p, tagsSize));
			p += tagsSize;
		}
	}

	void readWays(const uint8_t*& p)
	{
		int64_t prevId = 0;
		for (;;)
		{
			int64_t id = readVarint64(p);
			if (id == 0) break;
			bool isMultiTile = (id & 1);
			id = prevId + (id >> 1);
			prevId = id;
			ParentTileLocator locator;
			if (isMultiTile) locator = *p++;
			uint32_t bodySize = readVarint32(p);
			self().way(id, locator, ByteSpan(p, bodySize));
			p += bodySize;
		}
	}

	void readRelations(const uint8_t*& p)
	{
		int64_t prevId = 0;
		for (;;)
		{
			int64_t id = readVarint64(p);
			if (id == 0) break;
			bool isMembership = id & 1;
			id = prevId + (id >> 1);
			prevId = id;
			int locator = *p++;
			if (isMembership)
			{
				TypedFeatureId typedMemberId = TypedFeatureId(readVarint64(p));
				self().membership(id, locator, typedMemberId);
			}
			else
			{
				uint32_t bodySize = readVarint32(p);
				self().relation(id, locator, ByteSpan(p, bodySize));
				p += bodySize;
			}
		}
	}

	// TODO: Don't generate FFR for nodes that have no tex!!!!!

	void readForeignNodes(const uint8_t*& p)
	{
		int sourcePile = readVarint32(p);
			// Don't combine these two lines, compiler may be 
			// tempted to optimize away (Validator doesn't care about TIPs,
			// so it does not override pileToTip(), which always returns 0
		Tip tip = self().pileToTip(sourcePile);
		int64_t prevId = 0;
		Coordinate xy(0,0);
		for (;;)
		{
			int64_t id = readVarint64(p);
			if (id == 0)
			{
				break;
			}
			ForeignFeatureRef ref;
			if (id & 1)
			{
				Tex tex = readVarint32(p);
				ref = ForeignFeatureRef(tip, tex);
			}
			id = prevId + (id >> 1);
			if (id < prevId)
			{
				// Console::msg("read node/%lld after node/%lld (ok)",	id, prevId);
			}
			prevId = id;
			xy.x += readSignedVarint32(p);
			xy.y += readSignedVarint32(p);
			self().foreignNode(id, xy, ref);
		}
	}

	void readForeignFeatures(FeatureType type, const uint8_t*& p)
	{
		int sourcePile = readVarint32(p);
		Tip tip = self().pileToTip(sourcePile);

		int64_t prevId = 0;
		int32_t prevX = 0;
		int32_t prevY = 0;
		for (;;)
		{
			int64_t id = readVarint64(p);
			if (id == 0) break;
			bool hasBounds = id & 1;
			id = prevId + (id >> 1);
			prevId = id;
			uint32_t tex = readVarint32(p);	
			Box bounds;
			if (hasBounds)
			{
				prevX += readSignedVarint32(p);
				prevY += readSignedVarint32(p);
				bounds.setMinX(prevX);
				bounds.setMinY(prevY);
				bounds.setMaxX(prevX + readVarint32(p));
				bounds.setMaxY(prevY + readVarint32(p));
			}
			self().foreignFeature(type, id, bounds, ForeignFeatureRef(tip, tex));
		}
	}

	void readSpecialNodes(const uint8_t*& p)
	{
		int64_t prevId = 0;
		for (;;)
		{
			int64_t id = readVarint64(p);
			if (id == 0) break;
			int specialNodeFlags = static_cast<int>(id) & 3;
			id = prevId + (id >> 2);
			prevId = id;
			self().specialNode(id, specialNodeFlags);
		}
	}

    Derived& self()
    {
        return *static_cast<Derived*>(this);
    }

	ReusableBlock data_;
};

