// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TesWriter.h"
#include "TesFlags.h"
#include <geodesk/feature/GlobalTagIterator.h>
#include <geodesk/feature/LocalTagIterator.h>
#include <geodesk/feature/MemberTableIterator.h>
#include <geodesk/feature/NodeTableIterator.h>
#include <geodesk/feature/RelationTableIterator.h>
#include "tile/model/TNode.h"
#include "tile/model/TWay.h"
#include "tile/model/TRelation.h"
#include <clarisma/util/log.h>
#include <tile/model/TExportTable.h>


// TODO: 
// - Switch to 0-based indexng (but can no longer use 0 to verify placement)
// - fix gatherSharedItems() -- wrong minimums
// - write shared reltables

TesWriter::TesWriter(TileModel& tile, Buffer* out) :
	tile_(tile),
	out_(out),
	prevXY_(tile.bounds().bottomLeft()),
	nodeCount_(0),
	wayCount_(0)
{
}


void TesWriter::write()
{
	// TODO: Header
	writeFeatureIndex();
	writeStrings();
	writeTagTables();
	writeRelationTables();  
	writeFeatures();
	out_.writeByte(0); // no removed features
	writeExportTable();
	out_.flush();
}


void TesWriter::writeFeatureIndex()
{
	assert(nodeCount_ == 0);
	assert(wayCount_ == 0);

	FeatureTable::Iterator iter = tile_.iterFeatures();
	while (iter.hasNext())
	{
		features_.push_back(SortedFeature(iter.next()));
	}
	std::sort(features_.begin(), features_.end());

	out_.writeVarint(features_.size());
	int prevType = 0;
	uint64_t prevId = 0;
	for (int i=0; i<features_.size(); i++)
	{
		const SortedFeature& feature = features_[i];
		int type = feature.typeCode();
		if (type != prevType)
		{
			if (type == 1)
			{
				nodeCount_ = i;
			}
			else
			{
				assert(type == 2);
				wayCount_ = i - nodeCount_;
				if(prevType==0)
				{
					// In case there are only relations (i.e. we go
					// from type 0 straight to type 2),
					// write an additional 0-terminator
					out_.writeByte(0);
				}
			}
			out_.writeByte(0);
			prevType = type;
			prevId = 0;		// ID space starts over 
		}
		uint64_t id = feature.id();
		out_.writeVarint(((id - prevId) << 1) | 1);
			// Bit 0: changed_flag
		prevId = id;
		feature.feature()->setLocation(i);
	}
	LOG("Wrote %d features.", features_.size());
}


void TesWriter::writeStrings()
{
	gatherSharedItems(tile_.strings(), 0, 127, TString::compareGeneric);		
		// TODO: wrong, should be 1???
		// But we gather all strings anyway
		// If a string exceeds 16M users, its counter could wrap to 0
		// TesWriter will never encounter unused strings if it uses
		// a Model that only loads tile data
	out_.writeVarint(sharedElements_.size());
	for (auto& e : sharedElements_)
	{
		TString* s = static_cast<TString*>(e);
		out_.writeBytes(s->data(), s->size());
		// LOG("STRING \"%s\"", s->string()->toString().c_str());
	}
	LOG("Wrote %d strings.", sharedElements_.size());
}


void TesWriter::writeTagTables()
{
	gatherSharedItems(tile_.tagTables(), 2, 127);
	out_.writeVarint(sharedElements_.size());
	for (const auto& e : sharedElements_)
	{
		writeTagTable(static_cast<TTagTable*>(e));
	}
	LOG("Wrote %d tag tables.", sharedElements_.size());
}


void TesWriter::writeRelationTables()
{
	gatherSharedItems(tile_.relationTables(), 2, 63);
	out_.writeVarint(sharedElements_.size());
	for (const auto& e : sharedElements_)
	{
		writeRelationTable(static_cast<TRelationTable*>(e));
	}
	LOGS << "Wrote " << sharedElements_.size() << " relation tables.";
}

// TODO: let caller clear sharedElements_?
template <typename T>
void TesWriter::gatherSharedItems(const ElementDeduplicator<T>& items, 
	int minUsers, size_t firstGroupSize, CompareFunc compare)
{
	assert (firstGroupSize == 127 || firstGroupSize == 63);
	sharedElements_.clear();
	auto iter = items.iter();
	while (iter.hasNext())
	{
		TSharedElement* item = iter.next();
		if (item->users() >= minUsers) sharedElements_.push_back(item);
	}

	std::sort(sharedElements_.begin(), sharedElements_.end(),
		[](const TSharedElement* a, const TSharedElement* b)
		{
			// Sort in descending order based on number of users
			return a->users() > b->users();
		});

	if (compare)
	{
		size_t start = 0;
		size_t end = std::min(firstGroupSize + 1, sharedElements_.size());
		while (start < end)
		{
			// Within each group, sort elements in their natural order
			std::sort(sharedElements_.begin() + start, 
				sharedElements_.begin() + end, compare);
			start = end;
			end = std::min(end * 128, sharedElements_.size());
		}
	}

	for (int i = 0; i < sharedElements_.size(); i++)
	{
		sharedElements_[i]->setLocation(i);
	}
}


/*
void TesWriter::writeTagValue(DataPtr p, int valueFlags)
{
	assert(valueFlags >= 0 && valueFlags <= 3);
	uint32_t value;
	if ((valueFlags & 2) == 0)		  // narrow value
	{
		value = p.getUnsignedShort();
	}
	else                              // wide value
	{
		value = p.getUnsignedIntUnaligned();
		if (valueFlags == 3)
		{
			// TODO:
			TElement::Handle handle = tile_.existingHandle(p + static_cast<int32_t>(value));
			TString* valueStr = tile_.getString(handle);
			assert(valueStr);
			value = valueStr->location();
		}
	}
	out_.writeVarint(value);
}
*/

void TesWriter::writeStringValue(TElement::Handle handle)
{
	TString* str = tile_.getString(handle);
	assert(str);
	out_.writeVarint(str->location());
}

void TesWriter::writeTagTable(const TTagTable* tags)
{
	assert(tags->anchor() <= tags->size() - 4);
	assert((tags->size() & 1) == 0);
	TagTablePtr pTags = tags->tags();
	out_.writeVarint(tags->size() | (tags->hasLocalTags() ? 1 : 0));
	if (tags->hasLocalTags())
	{
		out_.writeVarint(tags->anchor() >> 1);
		LocalTagIterator localTags(tags->handle(), pTags);
		while (localTags.next())
		{
			TString* keyStr = tile_.getString(localTags.keyStringHandle());
			assert(keyStr);
			out_.writeVarint((keyStr->location() << 2) | (localTags.flags() & 3));
			if (localTags.hasLocalStringValue())
			{
				writeStringValue(localTags.stringValueHandleFast());
			}
			else
			{
				out_.writeVarint(localTags.value());
			}
		}
	}

	uint32_t prevKey = 0;
	GlobalTagIterator globalTags(tags->handle(), pTags);
	while (globalTags.next())
	{
		uint32_t key = globalTags.key();
		assert((prevKey == 0) ? (key >= prevKey) : (key > prevKey));
			// Global keys must be unique and ascending
		out_.writeVarint(((key - prevKey) << 2) | (globalTags.keyBits() & 3));
		prevKey = key;
		if (globalTags.hasLocalStringValue())
		{
			writeStringValue(globalTags.stringValueHandleFast());
		}
		else
		{
			out_.writeVarint(globalTags.value());
		}
	}
}


void TesWriter::writeFeatures()
{
	for (const auto& feature : features_)
	{
		switch (feature.typeCode())
		{
		case 0:
			writeNode(reinterpret_cast<TNode*>(feature.feature()));
			break;
		case 1:
			writeWay(reinterpret_cast<TWay*>(feature.feature()));
			break;
		case 2:
			writeRelation(reinterpret_cast<TRelation*>(feature.feature()));
			break;
		}
	}
}

void TesWriter::writeStub(const TFeature* feature, int flags)
{
	TTagTable* tags = feature->tags(tile_);
	flags |= TesFlags::TAGS_CHANGED | TesFlags::GEOMETRY_CHANGED;
	flags |= (tags->users() > 1) ? TesFlags::SHARED_TAGS : 0;
	flags |= feature->isRelationMember() ? TesFlags::RELATIONS_CHANGED : 0;
	out_.writeByte(flags);

	if (flags & TesFlags::SHARED_TAGS)
	{
		out_.writeVarint(tags->location());
	}
	else
	{
		writeTagTable(tags);
	}

	if (flags & TesFlags::RELATIONS_CHANGED)
	{
		TRelationTable* rels = feature->parentRelations(tile_);
		if (rels->users() > 1)
		{
			// number of a shared reltable, with marker flag
			out_.writeVarint((rels->location() << 1) | 1);	
		}
		else
		{
			writeRelationTable(rels);
		}
	}
}


void TesWriter::writeNode(TNode* node)
{
	int flags = (node->flags() & FeatureFlags::WAYNODE) ?
		TesFlags::NODE_BELONGS_TO_WAY : 0;
	flags |= (node->flags() & FeatureFlags::SHARED_LOCATION) ?
		TesFlags::HAS_SHARED_LOCATION : 0;
	flags |= (node->flags() & FeatureFlags::EXCEPTION_NODE) ?
		TesFlags::IS_EXCEPTION_NODE : 0;
	writeStub(node, flags);

	Coordinate xy = node->xy();
	out_.writeSignedVarint(static_cast<int64_t>(xy.x) - prevXY_.x);
	out_.writeSignedVarint(static_cast<int64_t>(xy.y) - prevXY_.y);
	prevXY_ = xy;
}


void TesWriter::writeWay(TWay* way)
{
	WayPtr wayRef(way->feature());
	bool hasFeatureNodes = (wayRef.flags() & FeatureFlags::WAYNODE);
	int flags =
		(hasFeatureNodes ? TesFlags::MEMBERS_CHANGED : 0) |
		(wayRef.isArea() ? TesFlags::IS_AREA : 0); // |
		// TesFlags::NODE_IDS_CHANGED;   // TODO
	writeStub(way, flags);
	
	const TWayBody* body = way->body();
	DataPtr pBody = body->data();
	int anchor = body->anchor();

	// By re-encoding the first coordinate (rather than storing the bbox minX/minY,
	// followed by coordCount and coord-deltas, as they are stored in the GOL)
	// we reduce the size of the TES by 8%

	/*
	Coordinate xy = wayRef.bounds().bottomLeft();
	out_.writeSignedVarint(static_cast<int64_t>(xy.x) - prevXY_.x);
	out_.writeSignedVarint(static_cast<int64_t>(xy.y) - prevXY_.y);
	prevXY_ = xy;

	out_.writeBytes(pBody + anchor, body->size() - anchor);
	*/

	const uint8_t* p = pBody;
	size_t coordSize = body->size() - anchor;
	int coordCount = readVarint32(p);

	/*
	if (wayRef.id() == 27380719)
	{
		printf("!!!\n");
	}
	*/

	Box bounds = wayRef.bounds();
	assert(bounds.intersects(tile_.bounds()));
	/*
	
	Don't do this, evaluation order of function argumetns is unspecified, so
	the y-delta may be read first, which is wrong !!!!!!!!!!!!

	Coordinate first(
		bounds.minX() + readSignedVarint32(p),
		bounds.minY() + readSignedVarint32(p));
	
	*/

	int_fast32_t xDelta = readSignedVarint32(p);
	int_fast32_t yDelta = readSignedVarint32(p);
	Coordinate first(static_cast<int32_t>(bounds.minX() + xDelta),
        static_cast<int32_t>(bounds.minY() + yDelta));

	out_.writeVarint(coordCount);
	out_.writeSignedVarint(static_cast<int64_t>(first.x) - prevXY_.x);
	out_.writeSignedVarint(static_cast<int64_t>(first.y) - prevXY_.y);
	prevXY_ = first;
	coordSize -= p - pBody.ptr();
	out_.writeBytes(p, coordSize);

	if (hasFeatureNodes)
	{
		int skipReltablePointer = (wayRef.flags() & FeatureFlags::RELATION_MEMBER) ? 4 : 0;
		out_.writeVarint(anchor - skipReltablePointer);

		// TODO: This is unintuitive; we need to adjust both the handle
		//  and the pointer if a reltable is present
		NodeTableIterator iter(body->handle() - skipReltablePointer, pBody - skipReltablePointer);
		while (iter.next())
		{
			if (iter.isForeign())
			{
				uint32_t zigzagTexDelta = toZigzag(iter.texDelta());
				if (iter.isInDifferentTile())
				{
					out_.writeVarint((zigzagTexDelta << 2) | 3);
					out_.writeSignedVarint(iter.tipDelta());
				}
				else
				{
					out_.writeVarint((zigzagTexDelta << 2) | 1);
				}
			}
			else
			{
				TReferencedElement* wayNode = tile_.getElement(iter.localHandle());
				assert(wayNode);
				out_.writeVarint(wayNode->location() << 1);
			}
		}
	}
}

void TesWriter::writeRelation(TRelation* relation)
{
	RelationPtr relationRef(relation->feature());
	int flags =
		(relationRef.isArea() ? TesFlags::IS_AREA : 0) |
		TesFlags::MEMBERS_CHANGED | TesFlags::BBOX_CHANGED;
	writeStub(relation, flags);

	if(relationRef.id() == 15773270)
	{
		LOGS << "Writing relation/" << relationRef.id() << "\n";
	}

	const TRelationBody* body = relation->body();
	DataPtr pBody = body->data();

	writeBounds(relationRef);

	int anchor = body->anchor();
	out_.writeVarint(body->size() - anchor);

	MemberTableIterator iter(body->handle(), pBody);
	while (iter.next())
	{
		int rolechangedFlag = iter.hasDifferentRole() ? 2 : 0;
		if (iter.isForeign())
		{
			uint32_t zigzapTexDelta = toZigzag(iter.texDelta());
			if (iter.isInDifferentTile())
			{
				out_.writeVarint((zigzapTexDelta << 3) | 5 | rolechangedFlag);
				out_.writeSignedVarint(iter.tipDelta());
			}
			else
			{
				out_.writeVarint((zigzapTexDelta << 3) | 1 | rolechangedFlag);
			}
		}
		else
		{
			TFeature* member = reinterpret_cast<TFeature*>(
				tile_.getElement(iter.localHandle()));
			assert(member);
			if(relationRef.id() == 15773270)
			{
				LOGS << "- " << member->id() << "\n";
			}
			out_.writeVarint((member->location() << 2) | rolechangedFlag);
		}
		if (rolechangedFlag)
		{
			uint32_t roleValue;
			if (iter.hasGlobalRole())
			{
				roleValue = (iter.globalRoleFast() << 1) | 1;
			}
			else
			{
				TString* str = tile_.getString(iter.localRoleHandleFast());
				assert(str);
				roleValue = str->location() << 1;
			}
			out_.writeVarint(roleValue);
		}
	}
}


void TesWriter::writeBounds(FeaturePtr feature)
{
	const Box& bounds = feature.bounds();
	out_.writeSignedVarint(static_cast<int64_t>(bounds.minX()) - prevXY_.x);
	out_.writeSignedVarint(static_cast<int64_t>(bounds.minY()) - prevXY_.y);
	out_.writeVarint(static_cast<uint64_t>(
		static_cast<int64_t>(bounds.maxX()) - bounds.minX()));
	out_.writeVarint(static_cast<uint64_t>(
		static_cast<int64_t>(bounds.maxY()) - bounds.minY()));
	prevXY_ = bounds.bottomLeft();
}


void TesWriter::writeRelationTable(const TRelationTable* relTable)
{
	//LOG("Writing reltable #%d", relTable->handle());
	if (relTable->size() > 127)
	{
		LOG("Relation table with size %d", relTable->size());
	}
	out_.writeVarint(relTable->size());
	RelationTablePtr p (relTable->data());
	RelationTableIterator iter(relTable->handle(), p);
	bool seenForeign = false;
	bool seenTileChange = false;

	while (iter.next())
	{
		if (iter.isForeign())
		{
			uint32_t zigzapTexDelta = toZigzag(iter.texDelta());
			if (iter.isInDifferentTile())
			{
				out_.writeVarint((zigzapTexDelta << 1) | 1);
				out_.writeSignedVarint(iter.tipDelta());
				seenTileChange = true;
			}
			else
			{
				assert(seenForeign);	
				assert(seenTileChange);
					// The first foreign relation must always have the
					// different_tile flag set
				out_.writeVarint((zigzapTexDelta << 1) | 0);
			}
			seenForeign = true;
		}
		else
		{
			assert(relTable->size() != 6);
				// Can't have any locals in a size 6 table, which can
				// only contain a single foreign relation
			assert(!seenForeign);
				// Local relations must be ordered before foreign
			TRelation* rel = reinterpret_cast<TRelation*>(
				tile_.getElement(iter.localHandle()));
			assert(rel);
			assert(rel->location() >= 0 && rel->location() < features_.size());
			int relNumber = rel->location() - nodeCount_ - wayCount_;
			out_.writeVarint(relNumber << 1);
		}
	}
}


void TesWriter::writeExportTable()
{
	TExportTable* exports = tile_.exportTable();
	if(exports == nullptr)
	{
		out_.writeByte(0);
		return;
	}
	TFeature** features = exports->features();
	assert(features);
	size_t count = exports->count();
	out_.writeVarint(count << 1);
	for(int i=0; i<count; i++)
	{
		TFeature* feature = features[i];
		assert(feature);
		out_.writeVarint(feature->location());
	}
}


