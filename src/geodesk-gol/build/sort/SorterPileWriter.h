// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "PileWriter.h"

class SorterPileWriter : public PileWriter
{
public:
	SorterPileWriter(uint32_t tileCount)
	{
		Pile** pileIndex = new Pile * [tileCount + 1];
		memset(pileIndex, 0, sizeof(Pile*) * (tileCount + 1));
		pileIndex_.reset(pileIndex);
	}

	/*
	SorterPileWriter(SorterPileWriter&& other) noexcept :
		PileSet(std::move(other)),
		pileIndex_(std::move(other.pileIndex_))
	{
	}
	*/

	Pile* get(uint32_t pileNumber, int groupType)
	{
		Pile* pile = pileIndex_[pileNumber];
		if (!pile)
		{
			pile = createPile(pileNumber, groupType);
			pileIndex_[pileNumber] = pile;
		}
		return pile;
	}

	void writeNode(uint32_t pileNumber, uint64_t id, Coordinate xy, ByteSpan tags)
	{
		Pile* pile = get(pileNumber, ProtoGol::LOCAL_NODES);
		assert(id > pile->prevId_);
		uint8_t buf[32];
		// enough room for ID delta (with Bit 0 tagged),
		// x/y deltas, and optional tagsLength
		uint8_t* p = buf;
		bool hasTags = !tags.isEmpty();
		writeVarint(p, ((id - pile->prevId_) << 1) | static_cast<int>(hasTags));
		writeSignedVarint(p, static_cast<int64_t>(xy.x) - pile->prevCoord_.x);
		writeSignedVarint(p, static_cast<int64_t>(xy.y) - pile->prevCoord_.y);
		if (hasTags)
		{
			size_t tagsLen = tags.size();
			writeVarint(p, tagsLen);
			assert(p - buf <= sizeof(buf));
			write(pile, buf, p - buf);
			write(pile, tags);
		}
		else
		{
			assert(p - buf <= sizeof(buf));
			write(pile, buf, p - buf);
		}
		pile->prevId_ = id;
		pile->prevCoord_ = xy;
	}

	void writeWay(uint32_t pileNumber, uint64_t id, ParentTileLocator locator,
		ByteSpan nodes, uint32_t taggedNodeCount, ByteSpan tags)
	{
		Pile* pile = get(pileNumber, ProtoGol::LOCAL_WAYS);
		assert(id > pile->prevId_);
		uint8_t buf[32];
		// enough room for ID delta (with Bit 0 tagged),
		// optional locator, bodyLength, and nodeCount
		bool isMultiTile = !locator.isEmpty();
		uint8_t* p = buf;
		writeVarint(p, ((id - pile->prevId_) << 1) | static_cast<int>(isMultiTile));
		if (isMultiTile) *p++ = locator;
		size_t tagsLen = tags.size();
		size_t nodesLen = nodes.size();
		size_t nodeCountLen = varintSize(taggedNodeCount);
		writeVarint(p, nodeCountLen + nodesLen + tagsLen);
		writeVarint(p, taggedNodeCount);
		assert(p - buf <= sizeof(buf));
		write(pile, buf, p - buf);
		write(pile, nodes.data(), nodesLen);
		write(pile, tags);
		pile->prevId_ = id;
	}

	void writeRelation(uint32_t pileNumber, uint64_t id, ParentTileLocator locator,
		uint32_t memberCount, ByteSpan body, ByteSpan extraTags)
	{
		Pile* pile = get(pileNumber, ProtoGol::LOCAL_RELATIONS);
		assert(id > pile->prevId_);
		uint8_t buf[32];
		// enough room for ID delta (with Bit 0 used as flag),
		// locator, and bodyLength
		uint8_t* p = buf;
		writeVarint(p, (id - pile->prevId_) << 1);
		// Bit 0 = 0 indicates relation (vs. membership)
		*p++ = locator;
		size_t bodyLen = body.size() + extraTags.size();
		size_t memberCountLen = varintSize(memberCount);
		writeVarint(p, memberCountLen + bodyLen);
		writeVarint(p, memberCount);
		assert(p - buf <= sizeof(buf));
		write(pile, buf, p - buf);
		write(pile, body);
		write(pile, extraTags);
		pile->prevId_ = id;
	}

	void writeMembership(uint32_t pileNumber, uint64_t relId,
		ParentTileLocator locator, uint64_t typedMemberId)
	{
		Pile* pile = get(pileNumber, ProtoGol::LOCAL_RELATIONS);
		assert(relId >= pile->prevId_);
		// There can be multiple memberships for the same relation
		uint8_t buf[32];
		// enough room for ID delta (with Bit 0 used as flag),
		// locator, and typed member ID
		uint8_t* p = buf;
		writeVarint(p, ((relId - pile->prevId_) << 1) | 1);
		// Bit 0 = 1 indicates membership (vs. relation)
		*p++ = locator;
		writeVarint(p, typedMemberId);
		assert(p - buf <= sizeof(buf));
		write(pile, buf, p - buf);
		pile->prevId_ = relId;
	}

	void closePiles()
	{
		Pile* pile = firstPile_;
		while (pile)
		{
			writeByte(pile, 0);
			pileIndex_.get()[pile->number_] = nullptr;
			pile = pile->nextPile_;
		}
	}

private:
	std::unique_ptr<Pile* []> pileIndex_;
};
