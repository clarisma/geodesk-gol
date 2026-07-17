// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/util/log.h>
#include <clarisma/util/MutableDataPtr.h>
#include <clarisma/util/TaggedPtr.h>
#include "TesException.h"
#include "tile/model/MutableFeaturePtr.h"
#include "tile/model/TileModel.h"


class TesReader
{
public:
	explicit TesReader(TileModel& tile);
	void read(const uint8_t* data, size_t size);

private:
	void readFeatureIndex();
	TString* readString();
	void readStrings();
	TTagTable* readTagTable();
	void readTagTables();
	TRelationTable* readRelationTable();
	void readRelationTables();
	TRelationTable* readRelationTableContents(uint32_t size);
	//void encodeTagValue(TTagTable::Hasher& hasher, MutableDataPtr p, uint32_t keyBits);
	uint32_t readFeatureChange(TFeature* f, TRelationTable** pRels);
	void readFeatureChanges();
	void readNodeChange(TNode* node);
	void readWayChange(TWay* way);
	void readRelationChange(TRelation* rel);
	Coordinate readCoordinate(Coordinate prev);
	Coordinate readFirstCoordinate();
	Box readBounds();
	bool readWayNodeTable(TElement::Handle handle, uint8_t* pTable, uint32_t tableSize);
	void readRemovedFeatures();
	void readExports();
	void setGeometryFlags(MutableFeaturePtr pFeature, int tesFlags);

	TString* getString(int number) const;
	TTagTable* getTagTable(int number) const;
	TRelationTable* getRelationTable(int number) const;
	TFeature* getFeature(int number) const;
	TNode* getNode(int number) const;
	TRelation* getRelation(int number) const; 

	template <typename... Args>
	static void invalid(const char* message, Args... args) 
	{
		printf(message, args...);
		throw TesException(message, args...);
	}

	struct WayGeometry
	{
		Box bounds;
		uint8_t* copiedCoords;
		uint8_t newCoordData[32];
		uint32_t copiedSize;
		uint32_t newSize;
	};

	TileModel& tile_;
	const uint8_t* p_;
	TString** strings_;
	TTagTable** tagTables_;
	TRelationTable** relationTables_;
	TaggedPtr<TFeature,1>* features_[3];
	uint32_t featureCount_;
	uint32_t stringCount_;
	uint32_t sharedTagTableCount_;
	uint32_t sharedRelationTableCount_;
	Coordinate prevXY_;
};
