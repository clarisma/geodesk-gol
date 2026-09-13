// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "PileWriter.h"
#include "build/util/TileCatalog.h"
#include <geodesk/geom/Box.h>

class ValidatorPileWriter : public PileWriter
{
public:
	ValidatorPileWriter(const TileCatalog& tc): tileCatalog_(tc)
	{
		resetIndex();
	}

	void init(int currentPile, Tile currentTile)
	{
		currentPile_ = currentPile;
		uint8_t* p = currentPileEncoded_;
		writeVarint(p, currentPile);
		currentPileEncodedLen_ = static_cast<int>(p - currentPileEncoded_);
		assert(currentPileEncodedLen_ <= sizeof(currentPileEncoded_));
		currentTile_ = currentTile;
		resetIndex();
	}

	Pile* getLocal(int groupType)
	{
		Pile* pile = pileIndex_[0].ptr();
		if(pileIndex_[0].flags() == 0)
		{
			if (!pile)
			{
				pile = createPile(currentPile_, groupType);
			}
			else
			{
				writeByte(pile, groupType);
			}
			pileIndex_[0] = TaggedPtr<Pile,1>(pile,1);
		}
		return pile;
	}

	Pile* getForeign(int relativePile, int groupType)
	{
		assert(relativePile >= 0 && relativePile < MAX_EXPORT_TILES);
		Pile* pile = pileIndex_[relativePile].ptr();
		if(pileIndex_[relativePile].flags() == 0)
		{
			if (!pile)
			{
				int relZoom = relativePile / 5;
				int twinCode = relativePile % 5;
				assert(currentTile_.zoom() - relZoom >= 0);
				assert(twinCode >= 0 && twinCode <= 4);
				assert(twinCode == 0 || currentTile_.zoom() - relZoom > 0);
				// the root tile does not have neighbors
				Tile tile = currentTile_.zoomedOut(currentTile_.zoom() - relZoom).twin(twinCode);
				int pileNumber = tileCatalog_.pileOfTile(tile);
				assert(pileNumber);
				pile = createPile(pileNumber, groupType);
			}
			else
			{
				writeByte(pile, groupType);
			}
			write(pile, currentPileEncoded_, currentPileEncodedLen_);
			pileIndex_[relativePile] = TaggedPtr<Pile,1>(pile,1);
		}
		return pile;
	}

	void writeForeignNode(int relativePile, uint64_t id, Coordinate xy, int tex)
	{
		Pile* pile = getForeign(relativePile, ProtoGol::EXPORTED_NODES);
		uint8_t buf[32];
		// enough room for ID delta (with Bit 0 used as feature-node flag),
		// x/y deltas, and optional tex
		uint8_t* p = buf;
		bool hasTex = (tex >= 0);
		assert(id != pile->prevId_);
		if (id < pile->prevId_)
		{
			/*
			Console::msg("wrote node/%lld after node/%lld (ok)",
				id, pile->prevId_);
			*/
		}
		writeVarint(p, ((id - pile->prevId_) << 1) | static_cast<uint64_t>(hasTex));
		if (hasTex)
		{
			// only feature nodes need a TEX
			writeVarint(p, tex);
		}
		writeSignedVarint(p, static_cast<int64_t>(xy.x) - pile->prevCoord_.x);
		writeSignedVarint(p, static_cast<int64_t>(xy.y) - pile->prevCoord_.y);
		assert(p - buf <= sizeof(buf));
		write(pile, buf, p - buf);
		pile->prevId_ = id;
		pile->prevCoord_ = xy;
		/*
		if(id == 4418343161)
		{
			Console::msg("Exported node/%lld to pile %d", id, pile->number_);
		}
		*/
	}

	void writeForeignFeature(int relativePile, int type, uint64_t id, const Box& bounds, int tex)
	{
		Pile* pile = getForeign(relativePile, (type << 3) | ProtoGol::EXPORTED_GROUP);
		uint8_t buf[64];
		// enough room for ID delta (with Bit 0 used as bbox-flag),
		// tex, and optional bbox
		uint8_t* p = buf;
		bool hasBounds = !bounds.isEmpty();
		assert(id != pile->prevId_);	
		writeVarint(p, ((id - pile->prevId_) << 1) |
			static_cast<int>(hasBounds));
		writeVarint(p, tex);
		if (hasBounds)
		{
			writeSignedVarint(p, static_cast<int64_t>(bounds.minX()) - pile->prevCoord_.x);
			writeSignedVarint(p, static_cast<int64_t>(bounds.minY()) - pile->prevCoord_.y);
			writeVarint(p, static_cast<int64_t>(bounds.maxX()) -
				static_cast<int64_t>(bounds.minX()));
			writeVarint(p, static_cast<int64_t>(bounds.maxY()) -
				static_cast<int64_t>(bounds.minY()));
			pile->prevCoord_ = bounds.bottomLeft();
		}
		assert(p - buf <= sizeof(buf));
		write(pile, buf, p - buf);
		pile->prevId_ = id;
	}

	void writeSpecialNode(uint64_t id, int specialNodeFlags)
	{
		assert((specialNodeFlags & 3) == specialNodeFlags);
		Pile* pile = getLocal(ProtoGol::SPECIAL_GROUP);
		uint8_t buf[16];
		uint8_t* p = buf;
		writeVarint(p, ((id - pile->prevId_) << 2) | specialNodeFlags);
		assert(p - buf <= sizeof(buf));
		write(pile, buf, p - buf);
		pile->prevId_ = id;
	}

	void closePiles()
	{
		for (int i = 0; i < MAX_EXPORT_TILES; i++)
		{
			if (pileIndex_[i].flags())
			{
				Pile* pile = pileIndex_[i].ptr();
				writeByte(pile, 0);
				pile->prevId_ = 0;
				pile->prevCoord_ = Coordinate(0, 0);
				pileIndex_[i] = TaggedPtr<Pile, 1>(pile, 0);
			}
		}
	}

	// A tile can write to a maximum of 61 tiles (including to itself):
	// There are 13 levels (0-12); the tile on each level (except 0) 
	// has four neighbor tiles (N,W,S,E) -- 12 * 5 + 1 = 61
	static constexpr size_t MAX_EXPORT_TILES = 61;

private:
	void resetIndex()
	{
		memset(pileIndex_, 0, sizeof(pileIndex_));
	}

	const TileCatalog& tileCatalog_;
	int currentPile_;
	uint8_t currentPileEncoded_[4];
	int currentPileEncodedLen_;
	Tile currentTile_;
	TaggedPtr<Pile,1> pileIndex_[MAX_EXPORT_TILES];
		// Addressing is relative; the first entry is always the pile for
		// the current tile, followed by its 4 neighbors (N,W,S,E)
		// [5] is the immediate parent of the current tile (unless this
		// is the root tile), followed by the parent's neighbors, etc.
};
