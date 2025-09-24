// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileReader.h"
#include <clarisma/util/varint.h>
#include <geodesk/feature/MemberTableIterator.h>
#include <geodesk/feature/RelationTableIterator.h>
#include "tile/compiler/RelationTableHasher.h"
#include "tile/compiler/TagTableHasher.h"
#include "TNode.h"
#include "TWay.h"
#include "TRelation.h"

void TileReader::readTile(Tile tile, TilePtr pTile)
{
	base_ = pTile;
	tile_.setSource(pTile);
	tile_.init(tile, pTile.totalSize());
	readTileFeatures(pTile);

	DataPtr exports = pTile + TileConstants::EXPORTS_OFS;
	int32_t exportsRelPtr = exports.getInt();
	if(exportsRelPtr) readExportTable(exports + exportsRelPtr);

	// Now we have all features, tag-tables and strings in the old tile,
	// indexed by location (also indexed by ID for features)
}


void TileReader::readNode(NodePtr node)
{
	// LOGS << "Reading node/" << node.id();
	readTagTable(node);
	if (node.isRelationMember()) readRelationTable(node.bodyptr());
	TNode* n = tile_.addNode(handleOf(node), node);
	n->setOriginal(true);
#ifdef _DEBUG
	counts_.featureCount++;
#endif
}


void TileReader::readWay(WayPtr way)
{
	bool needsFixup = false;
	// LOG("Reading %s in %s...", way.toString().c_str(), tile_.toString().c_str());
	// LOG("Reading %s (#%d)", way.toString().c_str(), handleOf(way));
	readTagTable(way);
	DataPtr pBody = way.bodyptr();
	// LOG("  Body = %p", pBody.ptr());
	uint32_t relTablePtrSize = way.flags() & 4;
	uint32_t anchor;
	if (way.flags() & FeatureFlags::WAYNODE)
	{
		DataPtr pNode(pBody);
		pNode -= relTablePtrSize;		// skip pointer to reltable (4 bytes)
		for (;;)
		{
			pNode -= 2;
			uint16_t wayNodeFlags = pNode.getUnsignedShort();
			if (wayNodeFlags & MemberFlags::FOREIGN)
			{
				// move extra 2 bytes if wide-tex delta
				pNode -= (wayNodeFlags & (1 << 3)) >> 2;
				if (wayNodeFlags & (1 << 2))
				{
					// foreign node in different tile
					pNode -= 2;
					pNode -= (pNode.getShort() & 1) << 1;
					// move backward by 2 or 4 bytes, depending on
					// whether wide-tip delta flag (bit 0 is set)
				}
			}
			else
			{
				pNode -= 2;		// local node is always 4 bytes
				needsFixup = true;
			}
			if (wayNodeFlags & MemberFlags::LAST) break;
		}
		anchor = static_cast<uint32_t>(pBody - pNode);
	}
	else
	{
		anchor = relTablePtrSize;
	}

	const uint8_t* p = pBody;
	int nodeCount = readVarint32(p);
	skipVarints(p, nodeCount * 2);		// (coordinate pairs)
	uint32_t size = static_cast<uint32_t>(p - pBody.ptr() + anchor);
	if (relTablePtrSize)
	{
		readRelationTable((pBody-4).followUnaligned());
	}
	TWay* w = tile_.addWay(way, pBody, size, anchor);
	w->setOriginal(true);
	w->setNeedsFixup(needsFixup);
#ifdef _DEBUG
	counts_.featureCount++;
#endif
}


void TileReader::readRelation(RelationPtr relation)
{
	// LOG("Reading relation/%ld", relation.id());
	bool needsFixup = false;
	readTagTable(relation);
	DataPtr pBody = relation.bodyptr();
	DataPtr p = pBody;

	for (;;)
	{
		int memberFlags = p.getUnsignedShort();
		if (memberFlags & MemberFlags::FOREIGN)
		{
			// foreign member
			// move extra 2 bytes if wide-tex delta
			p += 2 + ((memberFlags & (1 << 4)) >> 3);
			if (memberFlags & (1 << 3))
			{
				// foreign member in different tile
				p += 2 + ((p.getShort() & 1) << 1);
				// move forward by 2 or 4 bytes, depending on
				// whether wide-tip delta flag (bit 0 is set)
			}
		}
		else
		{
			p += 4;
			needsFixup = true;
		}
		if (memberFlags & MemberFlags::DIFFERENT_ROLE)
		{
			int32_t rawRole = p.getUnsignedShort();
			if ((rawRole & 1) == 0) // local-string role?
			{
				rawRole = p.getIntUnaligned();
				// LOG("Reading role string...");
				readString(p + (rawRole >> 1));   // signed
				needsFixup = true;
				p += 2;
			}
			p += 2;
		}
		if (memberFlags & MemberFlags::LAST) break;
	}

	uint32_t size = p - pBody;
	if (relation.flags() & FeatureFlags::RELATION_MEMBER)
	{
		readRelationTable((pBody - 4).followUnaligned());	
		size += 4;
	}
	TRelation* r = tile_.addRelation(relation, pBody, size);
	r->setOriginal(true);
	r->setNeedsFixup(needsFixup);
#ifdef _DEBUG
	counts_.featureCount++;
#endif
}

TString* TileReader::readString(DataPtr p)
{
	TElement::Handle handle = handleOf(p);
	// LOG("Reading string #%d: \"%s\"", handle,
	//	reinterpret_cast<ShortVarString*>(p.ptr())->toString().c_str());
	TString* str = tile_.getString(handle);
	if (!str)
	{
		str = tile_.addUniqueString(handle, reinterpret_cast<ShortVarString*>(p.ptr()));
#ifdef _DEBUG
		counts_.stringCount++;
#endif
	}
	str->addUser();
	return str;
}


TTagTable *TileReader::readTagTable(FeaturePtr feature)
{
	TTagTable* tags = readTagTable(feature.tags());
	tags->addUser();
	return tags;
}


// Hash is calculated as follows: 
// local tags (traversal order), then global tags (traversal order),

// TODO: Document that this does not add user count

// TODO: needsFixup flag
TTagTable* TileReader::readTagTable(TagTablePtr pTagTable)
{
	DataPtr pTags = pTagTable.ptr();
	bool hasLocalTags = pTagTable.hasLocalKeys();
	TTagTable* tags = tile_.getTags(handleOf(pTags));
	if (tags) return tags;

	bool needsFixup = false;
	TagTableHasher hasher;

	uint32_t anchor;
	if (hasLocalTags)
	{
		needsFixup = true;
		DataPtr p = pTags;
		DataPtr origin = p & 0xffff'ffff'ffff'fffcULL;
		for (;;)
		{
			p -= 4;
			int32_t key = p.getIntUnaligned();
			int flags = key & 7;
			DataPtr pKeyString = origin + ((key ^ flags) >> 1);
			TString* keyString = readString(pKeyString);
			// Force string to be 4-byte aligned
			keyString->setAlignment(TElement::Alignment::DWORD);

			hasher.addKey(keyString);
			p -= 2 + (flags & 2);
			if (flags & 2) // wide value?
			{
				if (flags & 1) // wide-string value?
				{
					hasher.addValue(readString(p.followUnaligned()));
				}
				else
				{
					hasher.addValue(p.getUnsignedIntUnaligned());
				}
			}
			else
			{
				hasher.addValue(p.getUnsignedShort());
			}
			if (flags & 4) break;  // last-tag?
		}
		anchor = static_cast<uint32_t>(pTags - p);
	}
	else
	{
		anchor = 0;
	}

	uint32_t size;
	DataPtr p = pTags;
	for (;;)
	{
		uint16_t key = p.getUnsignedShort();
		hasher.addKey((key & 0x7fff) >> 2);
		p += 2;
		if (key & 2)	// wide value?
		{
			if (key & 1)	// wide-string value?
			{
				hasher.addValue(readString(p.followUnaligned()));
				needsFixup = true;
			}
			else
			{
				hasher.addValue(p.getUnsignedIntUnaligned());
			}
		}
		else
		{
			hasher.addValue(p.getUnsignedShort());
		}
		p += (key & 2) + 2;
		if (key & 0x8000) break;	// last global key
	}
	size = static_cast<uint32_t>(p - pTags + anchor);

#ifdef _DEBUG
	counts_.tagTableCount++;
#endif

	tags = tile_.addTagTable(handleOf(pTags),
		pTags, size, static_cast<uint32_t>(hasher.hash()), anchor);
	tags->setNeedsFixup(needsFixup);
	tags->setOriginal(true);

	// LOG("Read tags %s with hash %d", tags->toString(tile_).c_str(), tags->hash());
	return tags;
}


TRelationTable* TileReader::readRelationTable(DataPtr pTable)
{
	bool needsFixup = false;
	TElement::Handle handle = handleOf(pTable);
	TRelationTable* rels = tile_.getRelationTable(handle);
	if (!rels)
	{
		RelationTableHasher hasher;
		RelationTableIterator iter(handle, RelationTablePtr(pTable));
		while (iter.next())
		{
			if (iter.isForeign())
			{
				if (iter.isInDifferentTile())
				{
					hasher.addTipDelta(iter.tipDelta());
				}
				hasher.addTexDelta(iter.texDelta());
			}
			else
			{
				hasher.addLocalRelation(iter.localHandle());
				needsFixup = true;
			}
		}
		uint32_t size = DataPtr::nearDelta(iter.ptr() - pTable);
		rels = tile_.addRelationTable(handle, pTable, size,
			static_cast<uint32_t>(hasher.hash()));
		rels->setNeedsFixup(needsFixup);
		rels->setOriginal(true);	// TODO: needed?

		assert(rels->size() == size);
		// assert(size != 6 || !needsFixup);	// TODO: check!!!!
	}
	rels->addUser();
	return rels;
}


void TileReader::readExportTable(DataPtr p)
{
	uint32_t count = (p - 4).getUnsignedInt();
	assert(count);
	TFeature** features = tile_.arena().allocArray<TFeature*>(count);
	for(int i=0; i<count; i++)
	{
		TElement::Handle handle = handleOf(p.follow());
		features[i] = TFeature::cast(tile_.getElement(handle));
		assert(features[i]);
		p += 4;
	}
	tile_.createExportTable(features, nullptr, count);
}