// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TesReader.h"
#include "TesException.h"
#include "TesFlags.h"
#include "tile/model/MutableFeaturePtr.h"
#include "tile/model/TNode.h"
#include "tile/model/TWay.h"
#include "tile/model/TRelation.h"
#include "tile/compiler/MemberTableWriter.h"
#include "tile/compiler/NodeTableWriter.h"
#include "tile/compiler/RelationTableHasher.h"
#include "tile/compiler/RelationTableWriter.h"
#include "tile/compiler/TagTableWriter.h"
#include <clarisma/util/log.h>
#include <clarisma/util/varint.h>


TesReader::TesReader(TileModel& tile) :
	tile_(tile),
	p_(nullptr),
	strings_(nullptr),
	tagTables_(nullptr),
	relationTables_(nullptr),
	featureCount_(0),
	stringCount_(0),
	sharedTagTableCount_(0),
	sharedRelationTableCount_(0),
	prevXY_(tile.bounds().bottomLeft())
{
}


void TesReader::read(const uint8_t* data, size_t size)
{
	p_ = data;
	// TODO: set size so we can check against overruns
	readFeatureIndex();
	readStrings();
	readTagTables();
	readRelationTables();  
	readFeatureChanges();
	readRemovedFeatures();
	readExports();
}


void TesReader::readFeatureIndex()
{
	featureCount_ = readVarint32(p_);
	features_[0] = tile_.arena().allocArray<TaggedPtr<TFeature, 1>>(featureCount_);
	TaggedPtr<TFeature, 1>* end = features_[0] + featureCount_;
	features_[1] = end;		// in case there are no ways
	features_[2] = end;		// in case there are no relations
	TaggedPtr<TFeature, 1>* ppFeature = features_[0];
	int type = 0;
	uint64_t prevId = 0;
	while (ppFeature < end)
	{
		uint64_t ref = readVarint64(p_);
		if (ref == 0)
		{
			type++;
			features_[type] = ppFeature;
			prevId = 0;
			continue;
		}
		uint64_t id = (ref >> 1) + prevId;
		int changeFlag = static_cast<int>(ref) & 1;
		TFeature* feature = tile_.getFeature(static_cast<FeatureType>(type), id);
		if (!feature)
		{
			// If feature has not been marked as changed and it does not exist,
			// we have a referential integrity problem
			// However, don't report as error yet, because it may get resolved
			// (Reapplying updates may cause this)

			// LOGS << "Creating " << TypedFeatureId::ofTypeAndId(static_cast<FeatureType>(type), id);
			feature = tile_.createFeature(static_cast<FeatureType>(type), id);
			if (changeFlag && type==1 && id==30910986)
			{
				LOGS << feature->typedId() << " marked as changed, but does not exist";
			}
		}
		*ppFeature++ = TaggedPtr<TFeature,1>(feature, changeFlag);
		prevId = id;
	}
	LOGS << "Read " << featureCount_ << " features.";
}


void TesReader::readStrings()
{
	stringCount_ = readVarint32(p_);
	strings_ = tile_.arena().allocArray<TString*>(stringCount_);
	for (uint32_t i = 0; i < stringCount_; i++)
	{
		strings_[i] = readString();
	}
	LOGS << "Read " << stringCount_ << " strings.";
}

TString* TesReader::readString()
{
	const ShortVarString* s = reinterpret_cast<const ShortVarString*>(p_);
	uint32_t size = s->totalSize();
	TString* str = tile_.addString(s->toStringView());
	// LOG("STRING \"%s\"", reinterpret_cast<const ShortVarString*>(p_)->toString().c_str());
	p_ += size;
	return str;
}


TTagTable* TesReader::readTagTable()
{
	uint32_t taggedSize = readVarint32(p_);
	uint32_t size = taggedSize & 0xffff'fffe;
	uint32_t localTagsSize = 0;
	bool needsFixup = false;
	if (taggedSize & 1) // has local keys
	{
		needsFixup = true;
		localTagsSize = readVarint32(p_) << 1;
		if (localTagsSize > size - 4)
		{
			invalid("Size of locals(% d) too large for tag - table size % d",
				localTagsSize, size);
		}
		assert(localTagsSize <= size - 4);
	}
	TTagTable* tags = tile_.beginTagTable(size, localTagsSize);
	
	TagTableWriter writer(tags->handle(), tags->data());
	DataPtr pEnd = tags->data() - localTagsSize;
	while (writer.ptr() != pEnd)
	{
		uint32_t keyBits = readVarint32(p_);
		TString* keyString = getString(keyBits >> 2);
		keyString->setAlignment(TElement::Alignment::DWORD);

		// TODO: Need special tracking for key-strings since
		// some exisiting strings may be unaligned -- hence,
		// when we look them up via handle (which is expected to be
		// 4-byte aligned), we will miss such an aligned key
		// Need a separate lookup table

		uint32_t value = readVarint32(p_);
		int valueFlags = keyBits & 3;
		if (valueFlags == 3)
		{
			writer.writeLocalTag(keyString, getString(value));
		}
		else
		{
			writer.writeLocalTag(valueFlags, keyString, value);
		}
	}
	writer.endLocalTags();

	pEnd = tags->data() + size - localTagsSize;
	uint32_t prevKeyShifted = 0;
	do
	{
		// key is delta-coded
		uint32_t keyBits = readVarint32(p_) + prevKeyShifted;
		prevKeyShifted = keyBits & 0xfffc;
		int valueFlags = keyBits & 3;
		uint32_t value = readVarint32(p_);
		if(valueFlags == 3)
		{
			writer.writeGlobalTag(keyBits >> 2, getString(value));
			needsFixup = true;
		}
		else
		{
			writer.writeGlobalTag(valueFlags, keyBits >> 2, value);
		}
	}
	while (writer.ptr() != pEnd);
	writer.endGlobalTags();

	return tile_.completeTagTable(tags, static_cast<uint32_t>(writer.hash()), needsFixup);
}


void TesReader::readTagTables()
{
	sharedTagTableCount_ = readVarint32(p_);
	tagTables_ = tile_.arena().allocArray<TTagTable*>(sharedTagTableCount_);
	for (uint32_t i = 0; i < sharedTagTableCount_; i++)
	{
		tagTables_[i] = readTagTable();
	}
	LOGS << "Read " << sharedTagTableCount_ << " tag tables.";
}


TRelationTable* TesReader::readRelationTable()
{
	uint32_t size = readVarint32(p_);
	return readRelationTableContents(size);
}

TRelationTable* TesReader::readRelationTableContents(uint32_t size)
{
	TRelationTable* rels = tile_.beginRelationTable(size);
	bool isForeign = false;
	RelationTableWriter writer(rels->handle(), rels->data());
	DataPtr end = writer.ptr() + size;
	bool needsFixup = false;
	do
	{
		uint32_t rel = readVarint32(p_);
		if (rel & 1)
		{
			// different tile
			TipDelta tipDelta = readSignedVarint32(p_);
			TexDelta texDelta = fromZigzag(rel >> 1);
			isForeign = true;
				// In a RelationTable, local relations always come first;
				// so once we've seen the first foreign relation, all 
				// remaining ones will be foreign, as well
			writer.writeForeignRelation(tipDelta, texDelta);
		}
		else
		{
			if (isForeign)
			{
				TexDelta texDelta = fromZigzag(rel >> 1);
				writer.writeForeignRelation(texDelta);
			}
			else
			{
				TRelation* r = getRelation(rel >> 1);
				writer.writeLocalRelation(r);
				needsFixup = true;
			}
		}
	}
	while (writer.ptr() != end);
	writer.markLast();
	rels = tile_.completeRelationTable(rels, writer.hash(), needsFixup);
	return rels;
}

void TesReader::readRelationTables()
{
	uint32_t count = readVarint32(p_);
	LOGS << "Reading " << count << " relation tables.";
	relationTables_ = tile_.arena().allocArray<TRelationTable*>(count);
	for (uint32_t i = 0; i < count; i++)
	{
		relationTables_[i] = readRelationTable();
	}
	LOGS << "Read " << count << " relation tables.";
}


void TesReader::readFeatureChanges()
{
	// No need, constructor has already initialized prevXY_
	// prevXY_ = tile_.bounds().bottomLeft();
	TaggedPtr<TFeature,1>* pp = features_[0];
	TaggedPtr<TFeature, 1>* ppEnd = pp + featureCount_;
	while (pp < features_[1])
	{
		// NOLINTNEXTLINE: The cast is safe
		if(pp->flags()) readNodeChange(static_cast<TNode*>(pp->ptr()));
		pp++;
	}
	while (pp < features_[2])
	{
		// NOLINTNEXTLINE: The cast is safe
		if (pp->flags()) readWayChange(static_cast<TWay*>(pp->ptr()));
		pp++;
	}
	while (pp < ppEnd)
	{
		// NOLINTNEXTLINE: The cast is safe
		if (pp->flags()) readRelationChange(static_cast<TRelation*>(pp->ptr()));
		pp++;
	}
}

/**
 * - Reads a `FeatureChange` record: flags, tags and reltable.
 * - Makes the feature mutable
 * - If tags changed, sets the new tagtable
 *   TODO: add user count to new, and decrease user count of old?
 * - If relations changed, reads the new relartion table and sets/clears 
 *   member flag; if feature becomes a member for the first time, or is
 *   removed from all relations, adds RELTABLE_CREATED or RELTABLE_DROPPED to flags
 */
uint32_t TesReader::readFeatureChange(TFeature* f, TRelationTable** pRels)
{
	// LOGS << "Reading stub for " << f->typedId();
	MutableFeaturePtr pFeature = f->makeMutable(tile_);
	
	uint32_t flags = *p_++;
	if (flags & TesFlags::TAGS_CHANGED)
	{
		TTagTable* tags;
		if (flags & TesFlags::SHARED_TAGS)
		{
			tags = getTagTable(readVarint32(p_));
		}
		else
		{
			tags = readTagTable();
		}
		tags->addUser();
			// TODO: reduce user count of old?
		pFeature.setTags(f->handle(), tags);
		/*
		LOGS << "Setting tags of " << f->typedId() << "(handle=" << f->handle()
			<< ") to handle " << tags->handle();
		LOGS << f->typedId() << " has tags " <<	tags->toString(tile_);
		*/
	}
	else
	{
		/*
		LOGS << "  Tags of " << f->typedId()
			<< " are unchanged. Feature handle = " << f->handle()
			<< ", tags handle = " << f->tags(tile_)->handle();
		*/
	}

	*pRels = nullptr;
	if (flags & TesFlags::RELATIONS_CHANGED)
	{
		uint32_t relsSizeOrRef = readVarint32(p_);
		if (relsSizeOrRef != 0)	// 0 means feature no longer has a reltable
		{
			if (relsSizeOrRef & 1)
			{
				*pRels = getRelationTable(relsSizeOrRef >> 1);
			}
			else
			{
				*pRels = readRelationTableContents(relsSizeOrRef);
				// No need to shift, size is always a multiple of 2
				// and Bit 0 is cleared to signal that this is a private table
			}
			(*pRels)->addUser();
			// TODO: reduce user count of old?

			if (!pFeature.isRelationMember())
			{
				pFeature.setFlag(FeatureFlags::RELATION_MEMBER, true);
				flags |= TesFlags::RELTABLE_CREATED;    // TODO: not needed?
			}
		}
		else
		{
			if (pFeature.isRelationMember())
			{
				pFeature.setFlag(FeatureFlags::RELATION_MEMBER, false);
				flags |= TesFlags::RELTABLE_DROPPED;	// TODO: not needed?
			}
		}
	}
	return flags;
}

Coordinate TesReader::readCoordinate(Coordinate prev)
{
	// Read deltas first, because eval order of the args passed to Coordinate
	// constructor is unspecified
	int64_t xDelta = readSignedVarint64(p_);
	int64_t yDelta = readSignedVarint64(p_);
	return Coordinate(static_cast<int32_t>(static_cast<int64_t>(prev.x) + xDelta),
		static_cast<int32_t>(static_cast<int64_t>(prev.y) + yDelta));
}

Coordinate TesReader::readFirstCoordinate()
{
	Coordinate xy = readCoordinate(prevXY_);
	prevXY_ = xy;
	return xy;
}


Box TesReader::readBounds()
{
	Coordinate bottomLeft = readFirstCoordinate();
	uint64_t w = readVarint64(p_);
	uint64_t h = readVarint64(p_);
	return Box(bottomLeft.x, bottomLeft.y,
		static_cast<int32_t>(static_cast<int64_t>(bottomLeft.x) + w),
		static_cast<int32_t>(static_cast<int64_t>(bottomLeft.y) + h));
}

void TesReader::readNodeChange(TNode* node)
{
	TRelationTable* newRels;
	uint32_t flags = readFeatureChange(node, &newRels);
	MutableFeaturePtr pFeature(node->feature());
	if (flags & TesFlags::GEOMETRY_CHANGED)
	{
		Coordinate xy = readFirstCoordinate();
		if (!tile_.bounds().contains(xy))
		{
			invalid("node/%lld lies outside of tile", node->feature().id());
		}
		pFeature.setNodeXY(xy);
	}

	pFeature.setFlag(FeatureFlags::WAYNODE, (flags & TesFlags::NODE_BELONGS_TO_WAY) != 0);
	pFeature.setFlag(FeatureFlags::SHARED_LOCATION, (flags & TesFlags::HAS_SHARED_LOCATION) != 0);
	pFeature.setFlag(FeatureFlags::EXCEPTION_NODE, (flags & TesFlags::IS_EXCEPTION_NODE) != 0);

	if (flags & TesFlags::RELATIONS_CHANGED)
	{
		if (newRels)
		{
			pFeature.setNodeRelations(node->handle(), newRels);
			node->setSize(24);
		}
		else
		{
			node->setSize(20);
		}
	}
}

/**
 *  The layout of a WayBody:
 *                              |<--anchor
 *                              |     
 *     (nodeTable)|(relTablePtr)|nodeCount/firstXY|coords|(nodeIds)
 * 
 * - If NODE_IDS_CHANGED, we need to build the node table from the TES data;
 *   otherwise, we copy it from the old body
 * - If GEOMETRY_CHANGED, we read the nodeCount and first coordinate from the TES
 *   and calculate the bbox (the 1st coordinate of the way will be relative to 
 *   the bbox); we then copy the remaining coords from the TES
 *   - If NODE_IDS_CHANGED, we copy the node IDs from the TES (if the GOL accepts 
 *     them); otherwise, we copy them from the old body
 * - If not GEOMETRY_CHANGED, we copy the coords (and node IDs, if present) from
 *   the old body
 * 
 * TODO: What happens if only parts of a waybody are updated, but way does not exist?
 * (This could happen if way is deleted in Rev #3, add we've already applied that
 * revision; becuase of rev consolidation, we may need to re-apply Rev #2, in which 
 * the way is partially updated)
 * --> Don't apply partial updates to missing/deleted ways
 *    But we need to distinguish missing vs. deleted
 *    Deleted ways are still indexed (with their geometry & node IDs, but without
 *    node refs; they have their "deleted" flag set)
 *    Missing ways don't exist at all in the tile
 */
void TesReader::readWayChange(TWay* way)
{
	if (way->id() == 30910986)
	{
		LOGS << "Reading change for way/" << way->id();
	}

	TRelationTable* newRels;
	bool wasRelationMember = way->isRelationMember();
	uint32_t flags = readFeatureChange(way, &newRels);
	MutableFeaturePtr pFeature(way->feature());
	TWayBody* body = way->body();
	DataPtr pOldBody = body->data();
	bool needsFixup = body->needsFixup();

	// LOG("*** way/%lld:", way->feature().id());

	uint32_t nodeTableSize;
	uint32_t countAndFirstSize;
	const uint8_t* pCoords;
	uint32_t coordsSize;
	// We start with assumption that node IDs are not included, hence not 
	// copied from anywhere, so we set pointer and size to null/0
	const uint8_t* pNodeIds = nullptr;		
	size_t nodeIdsSize = 0;
	uint8_t countAndFirst[32];

	// bool includeWayNodeIds = false;
		// TODO: Take flag from settings in the GOL header
		// If the GOL contains waynode IDs, the TES must include node IDs as well
		// (The TES Reader must have checked that before this point)

	if (flags & TesFlags::GEOMETRY_CHANGED)
	{
		uint32_t coordCount = readVarint32(p_);
		// LOG("Prev coord = %d, %d", prevXY_.x, prevXY_.y);
		Coordinate first = readFirstCoordinate();
		// LOG("1st coord = %d, %d", first.x, first.y);
		pCoords = p_;
		Box bounds(first);
		Coordinate node = first;
		for (int i = 1; i < coordCount; i++)	// We don't start at 0 because we already have the 1st coord
		{
			node = readCoordinate(node);
			bounds.expandToInclude(node);
		}

		if (flags & TesFlags::NODE_IDS_CHANGED)
		{
			pNodeIds = p_;
			skipVarints(p_, coordCount);
			coordsSize = (tile_.wayNodeIds() ? p_ : pNodeIds) - pCoords;
			// We advance the pointer so IDs will be copied along with
			// the coords (no need to set pNodeIds/nodeIdsSize)
		}
		else
		{
			if (tile_.wayNodeIds())		[[unlikely]]
			{
				// We'll need to copy the unchanged waynode IDs from the old body
				const uint8_t* pOldCoords = pOldBody;

				// TODO: There may not be a body present (see note above)
				uint32_t oldCoordCount = readVarint32(pOldCoords);
				if (oldCoordCount != coordCount)
				{
					invalid(
						"way/%lld: Node count changed from %d to %d,"
						"but NODE_IDS_CHANGED flag is not set");
				}
				skipVarints(pOldCoords, coordCount * 2);
				pNodeIds = pOldCoords;	// correct
				skipVarints(pOldCoords, coordCount);
				nodeIdsSize = pOldCoords - pNodeIds;
				// Now pNodeIds/nodeIdsSize are set to the old node IDs
			}
			coordsSize = p_ - pCoords;
		}

		uint8_t* pNew = countAndFirst;
		writeVarint(pNew, coordCount);
		writeSignedVarint(pNew, static_cast<int64_t>(first.x) - bounds.minX());
		writeSignedVarint(pNew, static_cast<int64_t>(first.y) - bounds.minY());
		countAndFirstSize = pNew - countAndFirst;

		if (!bounds.intersects(tile_.bounds()))
		{
			invalid("Bbox of way/%lld lies outside of tile\nTile = %s\nBbox = %s", 
				way->feature().id(), tile_.bounds().toString().c_str(),
				bounds.toString().c_str());
		}
		pFeature.setBounds(bounds);
	}
	else
	{
		if (flags & TesFlags::NODE_IDS_CHANGED)
		{
			invalid(
				"If NODE_IDS_CHANGED is set, GEOMETRY_CHANGED"
				"must be set, as well");
		}
		countAndFirstSize = 0;

		// Since way geometry and node IDs are unchanged, we need to 
		// take all the coordinates (and node IDs, if GOL includes them)
		// from the old body data

		pCoords = pOldBody;
		coordsSize = body->size() - body->anchor();
	}

	// If relation table was created or dropped, we need to change the 
	// body's anchor, because the node table is located before the 
	// relation table pointer
	bool willBeRelationMember = pFeature.isRelationMember();
	TElement::Handle bodyHandle = body->handle();
	if (wasRelationMember != willBeRelationMember)
	{
		if (willBeRelationMember)
		{
			body->setHandle(bodyHandle + 4);
			bodyHandle += 4;
		}
		else
		{
			body->setHandle(bodyHandle - 4);
			bodyHandle -= 4;
		}
	}

	uint32_t memberTableSize;
	if (flags & TesFlags::MEMBERS_CHANGED)
	{
		// Node Table will be built using data from the TES
		memberTableSize = readVarint32(p_);
		if (memberTableSize == 0)
		{
			// The way no longer has a node table
			pFeature.setFlag(FeatureFlags::WAYNODE, false);
		}
		else
		{
			pFeature.setFlag(FeatureFlags::WAYNODE, true);
		}
	}
	else
	{
		// We need to copy the old Node Table
		memberTableSize = body->anchor() - (wasRelationMember ? 4 : 0);
	}

	uint32_t relationTablePtrSize = willBeRelationMember ? 4 : 0;
	uint32_t newAnchor = memberTableSize + relationTablePtrSize;
	uint32_t newBodySize = memberTableSize + relationTablePtrSize +
		countAndFirstSize + coordsSize + nodeIdsSize;
	DataPtr pNewBody = tile_.arena().alloc(newBodySize,	alignof(uint16_t)) + newAnchor;

	if(flags & TesFlags::MEMBERS_CHANGED)
	{
		if (memberTableSize > 0)
		{
			// TODO: spec change proposed, may need memberTableSize * 2
			needsFixup = readWayNodeTable(bodyHandle - relationTablePtrSize,
				pNewBody - relationTablePtrSize, memberTableSize);
		}
		else
		{
			needsFixup = false;
		}
	}
	else
	{
		memcpy(pNewBody.ptr() - newAnchor, pOldBody - body->anchor(), memberTableSize);
	}

	if (willBeRelationMember)
	{
		int32_t relTablePtr;
		if (flags & TesFlags::RELATIONS_CHANGED)
		{
			relTablePtr = newRels->handle() - body->handle() + 4;
		}
		else
		{
			relTablePtr = (pOldBody - 4).getIntUnaligned();
		}
		MutableDataPtr(pNewBody - 4).putIntUnaligned(relTablePtr);
	}
	
	MutableDataPtr p(pNewBody);
	p.putBytes(&countAndFirst, countAndFirstSize);
	p += countAndFirstSize;
	p.putBytes(pCoords, coordsSize);
	p += coordsSize;
	p.putBytes(pNodeIds, nodeIdsSize);
	
	setGeometryFlags(pFeature, flags);
	body->setData(pNewBody);
	body->setSize(newBodySize);
	body->setAnchor(newAnchor);
	body->setAlignment(((pFeature.flags() &
		(FeatureFlags::WAYNODE | FeatureFlags::RELATION_MEMBER)) == 0) ?
		TElement::Alignment::BYTE : TElement::Alignment::WORD);
	body->setNeedsFixup(needsFixup);
}

/**
 * Sets the AREA and NORTH/WEST flags for a way or relation.
 */
void TesReader::setGeometryFlags(MutableFeaturePtr pFeature, int tesFlags)
{
	Box tileBounds = tile_.bounds();
	pFeature.setFlag(FeatureFlags::AREA, (tesFlags & TesFlags::IS_AREA) != 0);
	pFeature.setFlag(FeatureFlags::MULTITILE_WEST, pFeature.minX() < tileBounds.minX());
	pFeature.setFlag(FeatureFlags::MULTITILE_NORTH, pFeature.maxY() > tileBounds.maxY());
}

// TODO: could track needsFixup in Writer
bool TesReader::readWayNodeTable(TElement::Handle handle, uint8_t* pTable, uint32_t tableSize)
{
	bool needsFixup = false;
	assert(tableSize > 0);
	NodeTableWriter writer(handle, pTable);
	DataPtr end = writer.ptr() - tableSize;
		// Remember, node table is built backwards!
	while (writer.ptr() > end)
	{
		uint32_t node = readVarint32(p_);
		if (node & 1)
		{
			// foreign node
			TexDelta texDelta = fromZigzag(node >> 2);
			if (node & 2)
			{
				// different tile
				TipDelta tipDelta = readSignedVarint32(p_);
				writer.writeForeignNode(tipDelta, texDelta);
			}
			else
			{
				writer.writeForeignNode(texDelta);
			}
		}
		else
		{
			TNode* tnode = getNode(node >> 1);
			writer.writeLocalNode(tnode);
			needsFixup = true;
		}
	}
	writer.markLast();
	return needsFixup;
}

// TODO: Is needsFixup affected by presence of reltable?
// (We should handle reltable differently, more efficient this way)
void TesReader::readRelationChange(TRelation* rel)
{
	if(rel->id() == 1183819)
	{
		LOG("Reading changes for relation/%lld", rel->feature().id());
	}

	TRelationTable* newRels;
	bool needsFixup = rel->needsFixup();

	uint32_t flags = readFeatureChange(rel, &newRels);
	MutableFeaturePtr pFeature(rel->feature());

	if (flags & TesFlags::BBOX_CHANGED)
	{
		pFeature.setBounds(readBounds());
	}

	// readFeatureChange() has already set the member flag to its post-update state
	uint32_t relsPtrSize = pFeature.isRelationMember() ? 4 : 0;
	TRelationBody* body = rel->body();
	DataPtr pOldBody = body->data();
	MutableDataPtr pBody;
	uint32_t tableSize;

	if (flags & TesFlags::MEMBERS_CHANGED)
	{
		tableSize = readVarint32(p_); 	// TODO: multiple by 2?
		assert((tableSize % 2) == 0);		// TODO: may change
	}
	else
	{
		if (flags & TesFlags::RELATIONS_CHANGED)
		{
			tableSize = body->size() - body->anchor();
		}
		else
		{
			// If neither members nor parent relations changed
			// (which means only geometry/bbox change), there is
			// no need to update the relation's body, so we're done
			return;
		}
	}
	uint32_t newBodySize = tableSize + relsPtrSize;
	pBody = tile_.arena().alloc(newBodySize, alignof(uint16_t)) + relsPtrSize;
	if (flags & TesFlags::RELATIONS_CHANGED)
	{
		if (newRels)
		{
			(pBody - 4).putIntUnaligned(newRels->handle() - body->handle() + 4);
		}
	}
	else
	{
		if (body->anchor())
		{
			// Copy the old reltable pointer (no need to rebase, because
			// the body's handle does not change
			(pBody - 4).putIntUnaligned((pOldBody - 4).getIntUnaligned());
		}
	}

	if (flags & TesFlags::MEMBERS_CHANGED)
	{
		MemberTableWriter writer(rel->body()->handle(), pBody);
		DataPtr end = writer.ptr() + tableSize;
		while (writer.ptr() < end)
		{
			uint32_t member = readVarint32(p_);
			int roleChangeFlag = (member & 2) ? MemberFlags::DIFFERENT_ROLE : 0;
			if (member & 1)
			{
				// foreign member
				TexDelta texDelta = fromZigzag(member >> 3);
				if (member & 4)
				{
					// different tile
					TipDelta tipDelta = readSignedVarint32(p_);
					writer.writeForeignMember(tipDelta, texDelta, roleChangeFlag);
				}
				else
				{
					writer.writeForeignMember(texDelta, roleChangeFlag);
				}
			}
			else
			{
				TFeature* memberFeature = getFeature(member >> 2);
				writer.writeLocalMember(memberFeature, roleChangeFlag);
				needsFixup = true;
			}
			if (roleChangeFlag)
			{
				uint32_t role = readVarint32(p_);
				if (role & 1)
				{
					writer.writeGlobalRole(role >> 1);
				}
				else
				{
					writer.writeLocalRole(getString(role >> 1));
					needsFixup = true;
				}
			}
		}
		writer.markLast();
	}
	else
	{
		// If no change in members (i.e. only the parent relations 
		// changed), copy the old member table. There's no need to
		// adjust pointers, as the body's handle remains the same
		memcpy(pBody.ptr(), pOldBody, body->size() - body->anchor());
	}

	setGeometryFlags(pFeature, flags);
	body->setData(pBody);
	body->setSize(newBodySize);
	body->setAnchor(pFeature.isRelationMember() ? 4 : 0);
	body->setNeedsFixup(needsFixup);
}

TString* TesReader::getString(int number) const
{
	// #ifdef GEODESK_SAFE
	if (number > stringCount_)
	{
		invalid("String #%d exceeds range (%d strings)", number, stringCount_);
	}
	// #endif
	return strings_[number];
}

TTagTable* TesReader::getTagTable(int number) const
{
	// #ifdef GEODESK_SAFE
	if (number > sharedTagTableCount_)
	{
		invalid("Tagtable #%d exceeds range (%d tagtables)", number, sharedTagTableCount_);
	}
	// #endif
	return tagTables_[number];
}

TRelationTable* TesReader::getRelationTable(int number) const
{
	// TODO: SAFE check range
	return relationTables_[number];
}


TFeature* TesReader::getFeature(int number) const
{
	if (number >= featureCount_)
	{
		invalid("Feature #%d exceeds range (%d features)", number, featureCount_);
	}
	return features_[0][number].ptr();
}


TNode* TesReader::getNode(int number) const
{
// #ifdef GEODESK_SAFE
	if (features_[0] + number >= features_[1])
	{
		invalid("Node #%d exceeds range (%d nodes)", number, features_[1] - features_[0]);
	}
// #endif
	TFeature* feature = features_[0][number].ptr();	
// #ifdef GEODESK_SAFE
	if (!feature->feature().isNode())
	{
		throw TesException("Feature #%d should be a node instead of %s",
			number, feature->feature().typeName());
	}
// #endif
	return static_cast<TNode*>(feature);
}

TRelation* TesReader::getRelation(int number) const
{
	if (features_[2] + number >= features_[0] + featureCount_)
	{
		invalid("Relation #%d exceeds range (%d relations)",
			number, featureCount_ - (features_[2] - features_[0]));
	}
	TFeature* feature = features_[2][number].ptr();
	if (!feature->feature().isRelation())
	{
		invalid("Feature #%d should be a relation instead of %s",
			number, feature->feature().typeName());
	}
	return static_cast<TRelation*>(feature);
}


void TesReader::readRemovedFeatures()
{
	uint32_t count = readVarint32(p_);
	int type = 0;
	uint64_t prevId = 0;
	while(count)
	{
		uint64_t ref = readVarint64(p_);
		if (ref == 0)
		{
			type++;
			prevId = 0;
			continue;
		}
		uint64_t id = (ref >> 1) + prevId;
		int deletedFlag = ref & 1;
		TFeature* feature = tile_.getFeature(static_cast<FeatureType>(type), id);
		if (feature)
		{
			// TODO: Set visibility/deleted
			// (If feature is not present, we do nothing)
		}
		prevId = id;
		count--;
	}
}

void TesReader::readExports()
{
	uint32_t taggedCount = readVarint32(p_);
	uint32_t count = taggedCount >> 1;
	if(count)
	{
		TFeature** features = tile_.arena().allocArray<TFeature*>(count);
		for(int i=0; i<count; i++)
		{
			uint32_t ref = readVarint32(p_);
			features[i] = getFeature(ref);
		}
		tile_.createExportTable(features, nullptr, count);
	}
}