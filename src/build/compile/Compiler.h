// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/data/HashMap.h>
#include <clarisma/thread/TaskEngine.h>
#include <geodesk/feature/FeatureStore_Transaction.h>
#include <geodesk/geom/Coordinate.h>
#include "build/util/ProtoGolReader.h"
#include "build/util/TileCatalog.h"
#include "tag/AreaClassifier.h"
#include "tile/model/TileModel.h"
#include "tile/model/TNode.h"
#include "ExportFile.h"
#include "FeatureRef.h"
#include "TagTableBuilder.h"

class Compiler;
class GolBuilder;
class TileCatalog;
class TNode;
class TFeature2D;
class TWay;
class TRelation;
class TTagTable;

#ifdef GOL_BUILD_STATS
struct TileStats
{
	TileStats()
	{
		clear();
	}

	void clear()
	{
		// memset(this, 0, sizeof(*this));
		grossExportedFeatureCount = 0;
		featureNodeCount = 0;
		grossWayCount = 0;
		grossRelationCount = 0;
		grossWayNodeCount = 0;
		grossFeatureWayNodeCount = 0;
		grossForeignWayNodeCount = 0;
		grossWideTexWayNodeCount = 0;
		grossMemberCount = 0;
		grossForeignMemberCount = 0;
		grossWideTexMemberCount = 0;
		grossParentRelationCount = 0;
		grossForeignParentRelationCount = 0;
		grossWideTexParentRelationCount = 0;
		importedFeatureCount = 0;
		importedNodeCount = 0;
	}

	TileStats& operator+=(const TileStats& other)
	{
		grossExportedFeatureCount += other.grossExportedFeatureCount;
		featureNodeCount += other.featureNodeCount;
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

	int64_t grossExportedFeatureCount{};
	int64_t featureNodeCount{};
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
#endif

class CompilerWorker : public ProtoGolReader<CompilerWorker>
{
public:
	explicit CompilerWorker(Compiler* compiler);

	// CRTP Overrides
	void node(uint64_t id, Coordinate xy, ByteSpan tags);
	void way(uint64_t id, ParentTileLocator locator, ByteSpan body);
	void relation(uint64_t id, ParentTileLocator locator, ByteSpan body);
	void membership(uint64_t relId, ParentTileLocator locator, TypedFeatureId typedMemberId);
	void foreignNode(uint64_t id, Coordinate xy, ForeignFeatureRef ref);
	void foreignFeature(FeatureType type, uint64_t id, const Box& bounds, ForeignFeatureRef ref);
	void specialNode(uint64_t id, int specialNodeFlags);
	void readExportTable(int count, const uint8_t*& p);
	Tip pileToTip(int pileNumber) const { return tileCatalog_.tipOfPile(pileNumber); }

	void processTask(int pile);
	void afterTasks() {}
	void harvestResults() {}

private:
	struct ForeignNode : ForeignFeatureRef
	{
		ForeignNode(Tip tip_, Tex tex_, Coordinate xy_) : 
			ForeignFeatureRef(tip_, tex_), xy(xy_) {}
		ForeignNode(ForeignFeatureRef ref, Coordinate xy_) :
			ForeignFeatureRef(ref), xy(xy_) {}

		Coordinate xy;
	};

	struct ForeignFeature
	{
		ForeignFeatureRef ref1;
		ForeignFeatureRef ref2;
		Box bounds;
	};

	struct WayNode : FeatureRef
	{
		WayNode(FeatureRef n, Coordinate xy_) : FeatureRef(n), xy(xy_) {}
		WayNode(ForeignNode foreign) :
			FeatureRef(static_cast<ForeignFeatureRef>(foreign)),
			xy(foreign.xy) {}

		TNode* local() const
		{
			return static_cast<TNode*>(FeatureRef::local());
		}

		Coordinate xy;
	};

	TTagTable* readTags(ByteSpan tags, bool determineIfArea)
	{
		return tagsBuilder_.getTagTable(tags, determineIfArea);
	}

	void buildNodes();
	void buildWays();
	void buildRelations();
	void buildWay(TWay* way);
	void buildRelation(TRelation* rel);
	void buildRelationTable(TFeature2D* feature);
	TNode* promoteAnonymousMemberNode(uint64_t nodeId);
	void addToBounds(TFeature* f, Box& bounds);
	void setBounds(MutableFeaturePtr feature, const Box& bounds);
	void reset();

	/*
	static ByteSpan duplicateNodeTags();
	static ByteSpan orphanNodeTags();
	static ByteSpan duplicateOrphanNodeTags();

	static char SPECIAL_NODE_TAGS[];
	*/

	Compiler* compiler_;
	const StringCatalog& strings_;
	const TileCatalog& tileCatalog_;
	TileModel tile_;
	int32_t tileMinX_;
	int32_t tileMaxY_;
	HashMap<uint64_t, Coordinate> coords_;
	HashMap<uint64_t, ForeignNode> foreignNodes_;
	HashMap<TypedFeatureId, ForeignFeature> foreignFeatures_;
	LinkedList<TNode> nodes_;
	LinkedList<TWay> ways_;
	LinkedList<TRelation> relations_;
	TagTableBuilder tagsBuilder_;
	std::vector<WayNode> wayNodes_;
	TTagTable* duplicateTags_;
	TTagTable* orphanTags_;
	bool includeWayNodeIds_;
		// TODO: should this be moved to TileModel?
	#ifdef GOL_BUILD_STATS
	TileStats stats_;
	#endif
};


class CompilerOutputTask
{
public:
	CompilerOutputTask() {} // TODO: only to satisfy compiler
	CompilerOutputTask(Tip tip, ByteBlock&& data) :
		data_(std::move(data)),
		tip_(tip)
	{}

	std::span<const uint8_t> data() const { return data_; }
	Tip tip() const { return tip_; }

private:
	ByteBlock data_;
	Tip tip_;
};


class Compiler : public TaskEngine<Compiler, CompilerWorker, int, CompilerOutputTask>
{
public:
	explicit Compiler(GolBuilder* builder);

	void compile();
	void processTask(CompilerOutputTask& task);

private:
	void initStore();
	std::unique_ptr<uint32_t[]> createIndexedKeySchema() const;
	ForeignFeatureRef lookupForeignRelation(Tile childTile, ParentTileLocator locator, uint64_t id);

	#ifdef GOL_BUILD_STATS
	void addStats(const TileStats& stats);
	static void reportStat(const char* s, int64_t count, int64_t baseCount = -1);
	#endif

	GolBuilder* builder_;
	AreaClassifier areaClassifier_;
	ExportFile exportFile_;
	double workPerTile_;
	FeatureStore store_;
	FeatureStore::Transaction transaction_;
	#ifdef GOL_BUILD_STATS
	TileStats stats_;
	std::mutex statsMutex_;
	int reportedTileCount_ = 0;
	#endif

	friend class CompilerWorker;
};
