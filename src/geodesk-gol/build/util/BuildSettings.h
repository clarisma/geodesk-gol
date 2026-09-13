// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <cstdint>
#include <vector>
#include <string_view>
#include <clarisma/validate/Validate.h>
#include <geodesk/feature/FeatureStore.h>
#include <geodesk/feature/ZoomLevels.h>
#include "tag/AreaClassifier.h"
#include "IndexedKey.h"


using namespace clarisma;
using namespace geodesk;

class BuildSettings
{
public:
	BuildSettings();

	static const uint32_t MAX_GLOBAL_STRING_CODE = (1 << 16) - 3;

	enum
	{
		AREA_TAGS,
		EXCLUDED_KEYS,
		ID_INDEXING,
		INDEXED_KEYS,
		KEY_INDEX_MIN_FEATURES,
		MAX_KEY_INDEXES,
		MAX_STRINGS,
		MAX_TILES,
		MIN_STRING_USAGE,
		MIN_TILE_DENSITY,
		PROPERTIES,
		RTREE_BRANCH_SIZE,
		SOURCE,
		THREADS,
		UPDATABLE,
		ZOOM_LEVELS,
	};

	#ifdef GEODESK_PYTHON

	typedef int (BuildSettings::* SetterMethod)(PyObject*);
	static const SetterMethod SETTER_METHODS[];

	int setOptions(PyObject* dict);
	int setAreaTags(PyObject* arg);
	int setExcludedKeys(PyObject* arg);
	int setIdIndexing(PyObject* arg);
	int setIndexedKeys(PyObject* arg);
	int setKeyIndexMinFeatures(PyObject* arg);
	int setMaxKeyIndexes(PyObject* arg);
	int setMaxStrings(PyObject* arg);
	int setMaxTiles(PyObject* arg);
	int setMinStringUsage(PyObject* arg);
	int setMinTileDensity(PyObject* arg);
	int setProperties(PyObject* arg);
	int setRTreeBranchSize(PyObject* arg);
	int setSource(PyObject* arg);
	int setThreads(PyObject* arg);
	int setUpdatable(PyObject* arg);
	int setZoomLevels(PyObject* arg);
	#endif

	const std::string& sourcePath() const { return sourcePath_; }
	uint32_t featurePilesPageSize() const { return featurePilesPageSize_; }
	/*
	const std::vector<std::string_view>& indexedKeyStrings() const
	{ 
		return indexedKeyStrings_; 
	}
	*/
	bool includeWayNodeIds() const { return includeWayNodeIds_; }
	const std::vector<IndexedKey>& indexedKeys() const { return indexedKeys_; }
	bool keepIndexes() const { return keepIndexes_; }
	bool keepWork() const { return keepWork_; }
	FeatureStore::IndexedKeyMap keysToCategories() const;
	int keyIndexMinFeatures() const { return keyIndexMinFeatures_; }
	int maxKeyIndexes() const { return maxKeyIndexes_; }
	int maxStrings() const { return maxStrings_; }
	int maxTiles() const { return maxTiles_; }
	int minStringUsage() const { return minStringUsage_; }
	int minTileDensity() const { return minTileDensity_; }
	int leafZoomLevel() const { return 12; }
	int rtreeBranchsize() const { return rtreeBranchSize_; }
	int threadCount() const { return threadCount_; }
	ZoomLevels zoomLevels() const { return zoomLevels_; }
	std::vector<AreaClassifier::Entry>& areaRules() { return areaRules_; };

	void setSource(std::string_view path);

	void setAreaRules(const char* rules);
	void setIndexedKeys(const char *s);

	void setIncludeWayNodeIds(bool b) { includeWayNodeIds_ = b; }
	void setKeepIndexes(bool b) { keepIndexes_ = b; }
	void setKeepWork(bool b) { keepWork_ = b; }
	
	void setKeyIndexMinFeatures(int v)
	{
		keyIndexMinFeatures_ = Validate::intValue(v, 0, 1'000'000);
	}

	void setLevels(const char *s);

	void setMaxKeyIndexes(int v)
	{
		maxKeyIndexes_ = Validate::intValue(v, 0, 30);
	}

	void setMaxStrings(int64_t v)
	{
		if (v < 256) v = 256;
		maxStrings_ = Validate::maxInt(v, MAX_GLOBAL_STRING_CODE + 1);
	}

	void setMaxTiles(int64_t v)
	{
		if (v < 1) v = 1;
		maxTiles_ = Validate::maxInt(v, 8'000'000);
	}

	void setMinTileDensity(int64_t v)
	{
		if (v < 1) v = 1;
		minTileDensity_ = Validate::maxInt(v, 10'000'000);
	}

	void setMinStringUsage(int64_t v)
	{
		if (v < 1) v = 1;
		minStringUsage_ = Validate::maxInt(v, 100'000'000);
	}

	void setRTreeBranchSize(int v)
	{
		rtreeBranchSize_ = Validate::intValue(v, 4, 255);
	}

	void setThreadCount(int64_t v)
	{
		if (v < 0) v = 0;
		threadCount_ = static_cast<int>(v);
	}

	void complete();

private:
	#ifdef GEODESK_PYTHON
	int addIndexedKey(PyObject* obj, int category);
	#endif
	void addIndexedKey(std::string_view key, int category);
	
	std::string sourcePath_;
	ZoomLevels zoomLevels_;
	int keyIndexMinFeatures_ = 300;
	int maxKeyIndexes_ = 8;
	int maxTiles_ = (1 << 16) - 1;
	int maxStrings_ = 32'000;
	int minStringUsage_ = 300;
	int minTileDensity_ = 75'000;
	int rtreeBranchSize_ = 16;
	int threadCount_ = 0;
	uint32_t featurePilesPageSize_ = 64 * 1024;
	//std::vector<std::string_view> indexedKeyStrings_;
	//std::vector<uint8_t> indexedKeyCategories_;
	std::vector<AreaClassifier::Entry> areaRules_;
	std::vector<IndexedKey> indexedKeys_;
	bool includeWayNodeIds_ = false;
	bool keepIndexes_ = false;
	bool keepWork_ = false;

	static const char DEFAULT_INDEXED_KEYS[];
};
