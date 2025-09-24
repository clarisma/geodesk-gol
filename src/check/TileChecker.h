// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <clarisma/data/HashMap.h>
#include <clarisma/data/HashSet.h>
#include <clarisma/validate/BinaryChecker.h>
#include <geodesk/feature/FeaturePtr.h>
#include <geodesk/feature/TilePtr.h>
#include <geodesk/feature/Tip.h>
#include <geodesk/geom/Tile.h>

using namespace clarisma;
using namespace geodesk;

class TileChecker : protected BinaryChecker
{
public:
    TileChecker(Tip tip, Tile tile, TilePtr pTile);

    bool check();

    struct Stats
	{
		Stats& operator+=(const Stats& other)
		{
			grossFeatureNodeCount += other.grossFeatureNodeCount;
			grossWayCount += other.grossWayCount;
			grossRelationCount += other.grossRelationCount;
			grossWayNodeCount += other.grossWayNodeCount;
			grossFeatureWayNodeCount += other.grossFeatureWayNodeCount;
			grossForeignWayNodeCount += other.grossForeignWayNodeCount;
			grossWideTexWayNodeCount += other.grossWideTexWayNodeCount;
			grossMemberCount += other.grossMemberCount;
			grossForeignMemberCount += other.grossForeignMemberCount;
			grossWideTexMemberCount += other.grossWideTexMemberCount;
			grossParentRelationCount += other.grossParentRelationCount;
			grossForeignParentRelationCount += other.grossForeignParentRelationCount;
			grossWideTexParentRelationCount += other.grossWideTexParentRelationCount;
			importedFeatureCount += other.importedFeatureCount;
			importedNodeCount += other.importedNodeCount;
			return *this;
		}

		int64_t grossFeatureNodeCount{};
		int64_t grossWayCount{};
		int64_t grossRelationCount{};
		int64_t grossWayNodeCount{};
		int64_t grossFeatureWayNodeCount{};
		int64_t grossForeignWayNodeCount{};
		int64_t grossWideTexWayNodeCount{};
		int64_t grossMemberCount{};
		int64_t grossForeignMemberCount{};
		int64_t grossWideTexMemberCount{};
		int64_t grossParentRelationCount{};
		int64_t grossForeignParentRelationCount{};
		int64_t grossWideTexParentRelationCount{};
		int64_t importedFeatureCount{};
		int64_t importedNodeCount{};
	};

private:
	struct TagTableInfo
	{
		static constexpr int TAGGED_DUPLICATE = 1;
		static constexpr int TAGGED_ORPHAN = 2;
		uint32_t keys = 0;
		uint32_t flags = 0;
	};

    void checkNodeIndex(DataPtr ppIndex);
    uint32_t checkNodeTrunk(DataPtr p, uint32_t keys, Box& actualBounds);
    uint32_t checkNodeLeaf(DataPtr p, uint32_t keys, Box& actualBounds);
    uint32_t checkNode(DataPtr p, Box& actualLeafBounds);
	void checkIndex(DataPtr ppIndex, FeatureTypes types);
    uint32_t checkTrunk(DataPtr p, FeatureTypes types, uint32_t keys, Box& actualBounds);
	uint32_t checkLeaf(DataPtr p, FeatureTypes types, uint32_t keys, Box& actualBounds);
	bool checkFeatureBounds2D(FeaturePtr feature);
	uint32_t checkFeature2D(FeaturePtr feature);
	uint32_t checkWay(DataPtr p);
	uint32_t checkRelation(DataPtr p);
	bool checkPointer(DataPtr pBase, int delta);
    bool checkAccess(DataPtr p, const char* what);
    bool checkBounds(DataPtr pStored);
    bool checkBounds(DataPtr pStored, const Box& actual);
    bool checkId(FeaturePtr feature);
    TagTableInfo checkTagTablePtr(DataPtr ppTags, TypedFeatureId typedId);
    TagTableInfo checkTagTable(DataPtr p, bool hasLocalTags, TypedFeatureId typedId);
    void checkTagValue(DataPtr p, int type);
    const ShortVarString* checkString(DataPtr p);
	void checkExports(DataPtr ppExports);

    static constexpr uint32_t INVALID_INDEX = 0xffff'ffff;

    Tip tip_;
    Tile tile_;
    Box tileBounds_;
	HashMap<DataPtr,TagTableInfo> tagTables_;
	HashSet<TypedFeatureId> features_;
	Stats stats_;
};

