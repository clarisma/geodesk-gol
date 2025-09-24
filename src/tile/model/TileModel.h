// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/alloc/Arena.h>
#include <clarisma/util/log.h>
#include <geodesk/feature/FeaturePtr.h>
#include <geodesk/feature/TilePtr.h>
#include <geodesk/feature/TypedFeatureId.h>
#include <geodesk/geom/Tile.h>
#include "TFeature.h"
#include "TRelationTable.h"
#include "TString.h"
#include "TTagTable.h"

class TExportTable;
class TNode;
class TWay;
class TRelation;

using namespace clarisma;
using namespace geodesk;

// TODO: Allow reuse of TileModel?

class TileModel
{
public:
	explicit TileModel();

	void setSource(TilePtr pTile)
	{
		pCurrentTile_ = pTile;
		currentTileSize_ = pTile.totalSize();
		nextNewHandle_ = (currentTileSize_ + 3) & 0xffff'fffc;
	}

	void init(Tile tile, size_t tileSize);
	void clear();
	
	TFeature* getFeature(TypedFeatureId typedId) const
	{
		return featuresById_.lookup(typedId.asIdBits());
	}

	TFeature* getFeature(FeatureType type, uint64_t id) const
	{
		return getFeature(TypedFeatureId::ofTypeAndId(type, id));
	}

	TNode* getNode(uint64_t id) const
	{
		return reinterpret_cast<TNode*>(getFeature(FeatureType::NODE, id));
	}

	Tile tile() const { return tile_; }

	TElement::Handle newHandle()
	{
		TElement::Handle h = nextNewHandle_;
		nextNewHandle_ += 4;
		return h;
	}

	TString* addString(std::string_view s);
	TString* addUniqueString(TElement::Handle handle, const ShortVarString* s);
	TTagTable* addTagTable(TElement::Handle handle, const uint8_t* data, 
		uint32_t size, uint32_t hash, uint32_t anchor);
	TRelationTable* addRelationTable(TElement::Handle handle, const uint8_t* data,
		uint32_t size, uint32_t hash);
	void scrapElement(TSharedElement* elem);

	TTagTable* beginTagTable(uint32_t size, uint32_t anchor);
	TTagTable* completeTagTable(TTagTable* tags, uint32_t hash, bool needsFixup);

	TRelationTable* beginRelationTable(uint32_t size);
	TRelationTable* completeRelationTable(TRelationTable* rels, uint32_t hash, bool needsFixup);

	TNode* addNode(TElement::Handle handle, NodePtr node);
	TWay* addWay(WayPtr way, DataPtr pBodyData, uint32_t bodySize, uint32_t bodyAnchor);
	TRelation* addRelation(RelationPtr rel, DataPtr pBodyData, uint32_t bodySize);
	
	TFeature* createFeature(FeatureType type, uint64_t id);

	/// Creates a new feature element of type T (a TFeature subtype:
	/// TNode, TWay or TRelation) along with a stub of type S (SNode or
	/// SFeature). The stub will be initialized with zeroes, and its
	/// header will have its id and type bits set.
	/// The feature is assigned a handle and is added to the FeatureIndex
	/// of the Model. Its data pointer points to the Header of the stub.
	/// This method assumes that a feature of the given type and ID does
	/// not yet exist.
	///
	template <typename T, typename S>
	T* createFeature(uint64_t id)
	{
		uint8_t* bytes = arena_.alloc(sizeof(T) + sizeof(S), alignof(T));
		TElement::Handle handle = newHandle();
		S* pFeatureStruct = reinterpret_cast<S*>(bytes + sizeof(T));
		memset(pFeatureStruct, 0, sizeof(S));
		pFeatureStruct->header =
			FeatureHeader::forTypeAndId(T::FEATURE_TYPE, id);
		T* feature = new(bytes) T(handle, pFeatureStruct->ptr());
		addFeatureToIndex(feature);
		return feature;
	}

	void createExportTable(TFeature** features, TypedFeatureId* typedIds, size_t count);

	TReferencedElement* getElement(TElement::Handle handle) const
	{
		return elementsByHandle_.lookup(handle);
	}

	TTagTable* getTags(TElement::Handle handle) const
	{
		// LOGS << "Getting tags for handle " << handle;
		return TElement::cast<TTagTable>(getElement(handle));
	}

	TString* getString(TElement::Handle handle) const
	{
		//LOG("Getting string at %d", handle);
		TString* str = TElement::cast<TString>(getElement(handle));
		assert(str==nullptr || str->anchor() == 0);
		return str;
	}

	TString* getKeyString(TElement::Handle handle) const;

	TRelationTable* getRelationTable(TElement::Handle handle) const
	{
		return TElement::cast<TRelationTable>(getElement(handle));
	}


	/*
	TString* getString(const uint8_t* s, uint32_t size) const
	{
		TString* s = strings_.reinterpret_cast<TString*>(elementsByLocation_.lookup(
			currentLocation(pointer(p))));
		assert(!s || s->type() == TElement::Type::STRING);
		return s;
	}
	*/


	Arena& arena() { return arena_; }
	Box bounds() const { return tile_.bounds(); }
	uint32_t featureCount() const { return featureCount_; }
	const FeatureTable& features() const { return featuresById_; }
	const ElementDeduplicator<TString>& strings() const { return strings_; }
	const ElementDeduplicator<TTagTable>& tagTables() const { return tagTables_; }
	const ElementDeduplicator<TRelationTable>& relationTables() const { return relationTables_; }
	TExportTable* exportTable() const { return exportTable_; }

	FeatureTable::Iterator iterFeatures() const
	{
		return featuresById_.iter();
	}

	uint8_t* newTileData() const { return pNewTile_;	}
	uint8_t* write(Layout& layout);

	/*
	TElement::Handle existingHandle(DataPtr p) const
	{
		TElement::Handle handle = static_cast<TElement::Handle>(p - pCurrentTile_.ptr());
		assert(handle > 0 && handle < currentTileSize_);
		return handle;
	}
	*/

	// TODO: If we keep an element count, we could pre-size this vector
	std::vector<TReferencedElement*> getElements() const
	{
		return elementsByHandle_.toVector();
	}

	static TilePtr changedTileBase(DataPtr data)
	{
		return TilePtr(data.ptr() - 0x3000'0000);
	}

#ifndef NDEBUG
	void check() const;
#endif

private:
	void addFeatureToIndex(TFeature* feature)
	{
		elementsByHandle_.insert(feature);
		featuresById_.insert(feature);
		featureCount_++;
	}

	// TString* addStringOrRollback(TString* str);

	Arena arena_;
	LookupByHandle elementsByHandle_;
	FeatureTable featuresById_;
	StringDeduplicator strings_;
	ElementDeduplicator<TTagTable> tagTables_;
	ElementDeduplicator<TRelationTable> relationTables_;
	TExportTable* exportTable_;
	TilePtr pCurrentTile_;
	uint8_t* pNewTile_;
	uint32_t currentTileSize_;
	TElement::Handle nextNewHandle_;
		// TODO: When updating tiles, we need to set this to the current size
		//  of the tile, to ensure that new elements are assigned unique handles
	uint32_t featureCount_;
	Tile tile_;
};


#ifndef NDEBUG
struct ElementCounts
{
	ElementCounts()
	{
		memset(this, 0, sizeof(*this));
	}

	void check(const ElementCounts& other) const
	{
		if (!check("features", featureCount, other.featureCount) ||
			// !check("strings", stringCount, other.stringCount) ||
			!check("tagTables", tagTableCount, other.tagTableCount))
		{
			assert(false);
		}
	}

	bool check(const char* s, int a, int b) const
	{
		if (a == b) return true;
		printf("Number of %s differs: %d vs. %d\n", s, a, b);
		return false;
	}

	ElementCounts& operator+=(const ElementCounts& other) noexcept
	{
		featureCount += other.featureCount;
		stringCount += other.stringCount;
		tagTableCount += other.tagTableCount;
		return *this;
	}

	void dump() const
	{
		printf("%d features\n", featureCount);
		printf("%d strings\n", stringCount);
		printf("%d tag tables\n", tagTableCount);
	}

	int featureCount;
	int stringCount;
	int tagTableCount;
};
#endif
