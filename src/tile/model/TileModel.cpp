// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileModel.h"
#include <algorithm>
#include "TExportTable.h"
#include "THeader.h"
#include "TIndexTrunk.h"
#include "TNode.h"
#include "TWay.h"
#include "TRelation.h"
#include "TString.h"
#include "Layout.h"
#include <clarisma/util/Crc32C.h>
#include <clarisma/util/log.h>
#include <clarisma/util/varint.h>
#include <geodesk/feature/FeatureStore.h>

TileModel::TileModel() :
	arena_(1024 * 1024, Arena::GrowthPolicy::GROW_50_PERCENT),
	exportTable_(nullptr),
	featureCount_(0),
	pCurrentTile_(nullptr),
	pNewTile_(nullptr),
	currentTileSize_(0),
	nextNewHandle_(4)
{
}

void TileModel::init(Tile tile, size_t tileSize)
{
	tile_ = tile;

	size_t minTableSize = 1;
	size_t tableSize = std::max(tileSize / 64 * 7, minTableSize);
	elementsByHandle_.init(arena_.allocArray<TReferencedElement*>(tableSize), tableSize);

	tableSize = std::max(tileSize / 512 * 37, minTableSize);
	featuresById_.init(arena_.allocArray<TFeature*>(tableSize), tableSize);

	tableSize = std::max(tileSize / 200, minTableSize);
	strings_.init(arena_.allocArray<TString*>(tableSize), tableSize);

	tableSize = std::max(tileSize / 90, minTableSize);
	tagTables_.init(arena_.allocArray<TTagTable*>(tableSize), tableSize);

	tableSize = std::max(tileSize / 3000, minTableSize);
	relationTables_.init(arena_.allocArray<TRelationTable*>(tableSize), tableSize);
}

void TileModel::clear()
{
	featureCount_ = 0;
	exportTable_ = nullptr;
	pCurrentTile_ = TilePtr(nullptr);
	pNewTile_ = nullptr;
	nextNewHandle_ = 4;
	currentTileSize_ = 0;
	arena_.clear();
}


// TODO: We could check if string exists without speculatively creating
// a TString and a copy of the string content, but in most cases,
// the string won't be a duplicate
// TODO: This *must* be a ShortVarString, change argument type
// Cannot pass a std::string_view
/*
TString* TileModel::addString(const uint8_t* stringData, uint32_t size)
{
	size_t allocSize = sizeof(TString) + size;
	uint8_t* bytes = arena_.alloc(allocSize, alignof(TString));
	uint8_t* newStringData = bytes + sizeof(TString);
	memcpy(newStringData, stringData, size);
	TString* newStr = new(bytes) TString(0, newStringData, size);
	return addStringOrRollback(newStr);
}
*/

TString* TileModel::addString(std::string_view s)
{
	// TODO: Must ensure that only the *characters* are hashed;
	//  previously hash also covered the length byt(s)
	uint32_t hash = Strings::hash(s.data(), s.size());
	TString* existingString = strings_.lookup(s, hash);
	if(existingString) return existingString;

	uint32_t totalStrSize = ShortVarString::totalSize(s.size());
	size_t allocSize = sizeof(TString) + totalStrSize;
	uint8_t* bytes = arena_.alloc(allocSize, alignof(TString));
	uint8_t* newStringData = bytes + sizeof(TString);
	ShortVarString* svs = reinterpret_cast<ShortVarString*>(newStringData);
	svs->init(s.data(), s.size());
	TString* newStr = new(bytes) TString(0, newStringData, totalStrSize, hash);
	strings_.insertUnique(newStr);
	newStr->setHandle(newHandle());
	elementsByHandle_.insert(newStr);
	return newStr;
}


TString* TileModel::addUniqueString(TElement::Handle handle, const ShortVarString* s)
{
	const char* chars = s->data();
	uint32_t len = s->length();
	uint32_t totalSize = s->totalSize();
	uint32_t hash = Strings::hash(chars, len);
	TString* str = arena_.create<TString>(handle,
		reinterpret_cast<const uint8_t*>(s), totalSize, hash);
	strings_.insertUnique(str);
	assert(handle == str->handle());
	elementsByHandle_.insert(str);
	return str;
}


TTagTable* TileModel::addTagTable(TElement::Handle handle,
	const uint8_t* data, uint32_t size, uint32_t hash, uint32_t anchor)
{
	TTagTable* tags = arena_.create<TTagTable>(handle, data, size, hash, anchor);
	assert(handle == tags->handle());
	// LOG("Inserting tags at %d", handle);
	elementsByHandle_.insert(tags);
	tagTables_.insertUnique(tags);
	return tags;
}

void TileModel::scrapElement(TSharedElement* elem)
{
	assert(elem->handle() == nextNewHandle_ - 4 &&
		"Other elements have been created since this element was created");
	nextNewHandle_ -= 4;	// "give back" the handle
	arena().freeLastAlloc(elem);
		// This assumes that the data of the element is always placed
		// *after* the element; otherwise, we're not giving back all 
		// of the allocated memory (This won't create a memory leak in
		// the classical sense, but consumes more memory than needed
		// for the lifetime of the Tilemodel)
}

TTagTable* TileModel::beginTagTable(uint32_t size, uint32_t anchor)
{
	uint8_t* bytes = arena_.alloc(sizeof(TTagTable) + size, alignof(TTagTable));
	return new(bytes) TTagTable (newHandle(), bytes + sizeof(TTagTable) + anchor,
		size, 0, anchor);
}

TTagTable* TileModel::completeTagTable(TTagTable* tags, uint32_t hash, bool needsFixup)
{
	// Fill in the hash field & fixup flags (needed for comparison)
	tags->setHash(hash);
	tags->setNeedsFixup(needsFixup);
	
	// Check if there is an identical TagTable already in the Model
	// If so, throw the speculatively constructed element away
	// (by rolling back the arena pointer) and return the old element

	TTagTable* existing = tagTables_.insert(tags);
	if (existing != tags)
	{
		scrapElement(tags);
		return existing;
	}
	// LOG("Created new tag table #%d", tags->handle());
	// LOG("Created tags %s with hash %d", tags->toString(*this).c_str(), tags->hash());
	elementsByHandle_.insert(tags);
	return tags;
}

TRelationTable* TileModel::addRelationTable(TElement::Handle handle, const uint8_t* data,
	uint32_t size, uint32_t hash)
{
	TRelationTable* rels = arena_.create<TRelationTable>(handle, data, size, hash);
	elementsByHandle_.insert(rels);
	relationTables_.insertUnique(rels);
	return rels;
}

TRelationTable* TileModel::beginRelationTable(uint32_t size)
{
	uint8_t* bytes = arena_.alloc(sizeof(TRelationTable) + size, alignof(TRelationTable));
	return new(bytes) TRelationTable(newHandle(), bytes + sizeof(TRelationTable), size, 0);
}

TRelationTable* TileModel::completeRelationTable(TRelationTable* rels, uint32_t hash, bool needsFixup)
{
	// Fill in the hash field
	rels->setHash(hash);
	rels->setNeedsFixup(needsFixup);

	// Check if there is an identical TagTable already in the Model
	// If so, throw the speculatively constructed element away
	// (by rolling back the arena pointer) and return the old element

	TRelationTable* existing = relationTables_.insert(rels);
	if (existing != rels)
	{
		scrapElement(rels);
		return existing;
	}
	elementsByHandle_.insert(rels);
	return rels;
}


TNode* TileModel::addNode(TElement::Handle handle, NodePtr node)
{
	TNode* tnode = arena_.create<TNode>(handle, node);
	addFeatureToIndex(tnode);
	return tnode;
}


TFeature* TileModel::createFeature(FeatureType type, uint64_t id)
{
	switch (type)
	{
	case FeatureType::NODE:
		return createFeature<TNode,SNode>(id);
	case FeatureType::WAY:
		return createFeature<TWay,SFeature>(id);
	case FeatureType::RELATION:
		return createFeature<TRelation,SFeature>(id);
	default:
		assert(false);
		return nullptr;
	}
}


TWay* TileModel::addWay(WayPtr way, DataPtr pBodyData, uint32_t bodySize, uint32_t bodyAnchor)
{
	TWay* tway = arena_.create<TWay>(
		pCurrentTile_, way, pBodyData, bodySize, bodyAnchor);
	addFeatureToIndex(tway);
	return tway;
}


TRelation* TileModel::addRelation(RelationPtr rel, DataPtr pBodyData, uint32_t bodySize)
{
	TRelation* trel = arena_.create<TRelation>(
		pCurrentTile_, rel, pBodyData, bodySize);
	addFeatureToIndex(trel);
	return trel;
}


void TileModel::createExportTable(TFeature** features, TypedFeatureId* typedIds, size_t count)
{
	assert(exportTable_ == nullptr);
	exportTable_ = arena_.create<TExportTable>(features, typedIds, count);
}

// TODO: don't return raw pointer
uint8_t* TileModel::write(Layout& layout)
{
	// LOG("Old size: %d | New size: %d", currentTileSize_, layout.size());

	pNewTile_ = new uint8_t[layout.size() + 8];
		// TODO +4 should be enough!
	MutableDataPtr p(pNewTile_);
	p.putUnsignedInt(layout.size());

	TElement* elem = layout.first();
	do
	{
		// LOG("%d: Type %d (%d bytes)", elem->location(), elem->type(), elem->size());
		switch (elem->type())
		{
		case TElement::Type::HEADER:
			static_cast<THeader*>(elem)->write(*this);
			break;
		case TElement::Type::NODE:
			static_cast<TNode*>(elem)->write(*this);
			break;
		case TElement::Type::FEATURE2D:
			static_cast<TFeature2D*>(elem)->write(*this);
			break;
		case TElement::Type::WAY_BODY:
			static_cast<TWayBody*>(elem)->write(*this);
			break;
		case TElement::Type::RELATION_BODY:
			static_cast<TRelationBody*>(elem)->write(*this);
			break;
		case TElement::Type::STRING:
			static_cast<TString*>(elem)->write(newTileData() + elem->location());
			break;
		case TElement::Type::TAGS:
			static_cast<TTagTable*>(elem)->write(*this);
			break;
		case TElement::Type::RELTABLE:
			static_cast<TRelationTable*>(elem)->write(*this);
			break;
		case TElement::Type::INDEX:
			static_cast<TIndex*>(elem)->write(*this);
			break;
		case TElement::Type::TRUNK:
			static_cast<TIndexTrunk*>(elem)->write(*this);
			break;
		case TElement::Type::EXPORTS:
			static_cast<TExportTable*>(elem)->write(*this);
			break;
		}
		assert(elem->next() == nullptr || elem->location() + elem->size() <= elem->next()->location());
		elem = elem->next();
	}
	while (elem);

	Crc32C checksum;
	checksum.update(pNewTile_, layout.size());
	(p + layout.size()).putUnsignedIntUnaligned(checksum.get());

	if (!FeatureStore::isTileValid(reinterpret_cast<std::byte*>(pNewTile_)))
	{
		Console::debug("Checksum calculation error");
	}
	return pNewTile_;
}


TString* TileModel::getKeyString(TElement::Handle handle) const
{
	TString* str = getString(handle);
	if (str == nullptr)
	{
		LOGS << "Can't find string with handle " << handle << ", probing nearby...";
		for (int ofs = -3; ofs <=3; ofs++)
		{
			str = getString(handle + ofs);
			if (str)
			{
				LOGS << "  Found string with handle " << handle
					<< " at " << (handle+ofs) << ": " << *str->string();
				return str;
			}
		}
		LOGS << "  Probe failed for string with handle " << handle;
	}
	return str;
}


#ifdef _DEBUG
void TileModel::check() const
{
	auto iter = elementsByHandle_.iter();
	while (iter.hasNext())
	{
		TReferencedElement* e = iter.next();
		if (e->location() <= 0)
		{
			if (e->type() == TElement::Type::TAGS)
			{
				TTagTable* tags = static_cast<TTagTable*>(e);
				LOG("Did not place tags %s (%d users)",
					tags->toString(*this).c_str(),
					tags->users());
			}
			if (e->type() == TElement::Type::STRING)
			{
				TString* str = static_cast<TString*>(e);
				LOG("Did not place string \"%s\" (%d users)",
					str->string()->toString().c_str(),
					str->users());
			}
			else
			{
				LOG("Did not place element of type %d", e->type());
			}
		}
	}
}
#endif