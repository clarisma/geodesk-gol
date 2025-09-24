// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <vector>
#include <unordered_set>
#include <clarisma/alloc/Arena.h>
#include <clarisma/data/Linked.h>
#include <clarisma/thread/Phaser.h>
#include <clarisma/util/BufferWriter.h>
#include <geodesk/geom/Coordinate.h>
#include "osm/OsmPbfReader.h"
#include "build/util/StringCatalog.h"
#include "FastFeatureIndex.h"
#include "SortedChildFeature.h"
#include "SorterPileWriter.h"

class GolBuilder;
class SuperRelation;

class Sorter;

struct SorterStatistics
{
	SorterStatistics()
	{
		memset(this, 0, sizeof(*this));
	}

	SorterStatistics& operator+=(const SorterStatistics& other)
	{
		nodeCount += other.nodeCount;
		wayCount += other.wayCount;
		relationCount += other.relationCount;
		superRelationCount += other.superRelationCount;
		emptyRelationCount += other.emptyRelationCount;
		refCycleCount += other.refCycleCount;
		multitileWayCount += other.multitileWayCount;
		ghostWayCount += other.ghostWayCount;
		wayNodeCount += other.wayNodeCount;
		memberCount += other.memberCount;
		foreignMemberCount += other.foreignMemberCount;
		return *this;
	}

	int64_t nodeCount;
	int64_t wayCount;
	int64_t multitileWayCount;
	int64_t ghostWayCount;
	int64_t wayNodeCount;
	int64_t relationCount;
	int64_t superRelationCount;
	int64_t emptyRelationCount;
	int64_t refCycleCount;
	int64_t memberCount;
	int64_t foreignMemberCount;
};

/*
class FeatureIndexEntry
{
public:
	FeatureIndexEntry(uint64_t id, uint32_t pile) :
		id_(id), pile_(pile) {}

	uint64_t id() const noexcept { return id_; }
	uint64_t pile() const noexcept { return pile_; }

private:
	uint64_t id_;
	uint32_t pile_;
};
*/

class SorterWorker : public OsmPbfContext<SorterWorker, Sorter>
{
public:
	explicit SorterWorker(Sorter* sorter);
	~SorterWorker();

	// TODO: This constructor only exists to satisfy the requirements
	// of std:vector (used in TaskEngine)
	// It must never be called!
	explicit SorterWorker(const SorterWorker& other) :
		OsmPbfContext(nullptr),
		tempBuffer_(4096),
		tempWriter_(&tempBuffer_),
		pileWriter_(0)
	{
		assert(false);
	}

	void setMainWorker() { isMainWorker_ = true; }

	LinkedQueue<SuperRelation> superRelations() const { return superRelations_; }

	// CRTP overrides
	void stringTable(ByteSpan strings);
	const uint8_t* node(int64_t id, int32_t lon100nd, int32_t lat100nd, ByteSpan tags);
	void beginWayGroup();
	void way(int64_t id, ByteSpan keys, ByteSpan values, ByteSpan nodes);
	void multiTileWay(int64_t id, ByteSpan nodes);
	void beginRelationGroup();
	void relation(int64_t id, ByteSpan keys, ByteSpan values,
		ByteSpan roles, ByteSpan memberIds, ByteSpan memberTypes);
	void endBlock();
	void afterTasks();
	void harvestResults() const;

private:
	void encodeTags(ByteSpan keys, ByteSpan values);
	const uint8_t* encodeTags(ByteSpan tags);
	void encodeString(uint32_t stringNumber, int type);
	// void writeWay(uint32_t pile, uint64_t id);
	void writeRelation(uint64_t id, int pilePair, TilePair tilePair,
		Span<SortedChildFeature> members, int highestMemberZoom,
		ByteSpan body, int missingMemberCount, int removedMemberCount);
	bool checkClosedRing(ByteSpan* nodes);

	void indexFeature(int64_t id, int pile);
	void advancePhase(int futurePhase);
	void flushPiles();
	void flushIndex();
	size_t batchSize(int phase) 
	{ 
		return phase == 0 ? (1024 * 1024) : (32 * 1024); 
	}  

	void deferSuperRelation(int64_t id, TilePair tentativeTilePair, int missingMembers);
	void resolveSuperRelations();

	GolBuilder* builder_;
	/**
	 * Pointer to the start of the string table of the current OSM block.
	 */
	const uint8_t* osmStrings_;

	/**
	 * This vector helps turn the string references in the current OSM block
	 * into proto-string encodings. For each string in the current OSM block,
	 * it holds an entry that turns a key or value into a varint (the code in
	 * the Proto-String Table) or into a literal string (an offset into the
	 * OSM block's string table)
	 */
	std::vector<ProtoStringPair> stringTranslationTable_;
	DynamicBuffer tempBuffer_;	// keep this order
	BufferWriter tempWriter_;
	SorterPileWriter pileWriter_;
	FastFeatureIndex indexes_[3];
	int currentPhase_;
	int pileCount_;

	std::vector<SortedChildFeature> children_;
	std::unordered_set<int> childPiles_;
	//std::vector<uint64_t> memberIds_;
	//std::vector<uint32_t> tagsOrRoles_;

	Arena superRelationData_;
	LinkedQueue<SuperRelation> superRelations_;

	SorterStatistics stats_;
	uint64_t batchCount_;
	bool isMainWorker_;
};

class SorterOutputTask : public OsmPbfOutputTask
{
public:
	SorterOutputTask() {} // TODO: not needed, only to satisfy compiler
	SorterOutputTask(uint64_t bytesProcessed, PileSet&& piles) :
		bytesProcessed_(bytesProcessed),
		piles_(std::move(piles))
	{
	}

	PileSet piles_;
	uint64_t bytesProcessed_;
};

class Sorter : public OsmPbfReader<Sorter, SorterWorker, SorterOutputTask>
{
public:
	enum Phase { NODES, WAYS, RELATIONS, SUPER_RELATIONS };

	explicit Sorter(GolBuilder* builder);
	GolBuilder* builder() const { return builder_; };
	void sort(const char* fileName);
	void startFile(uint64_t size);		// CRTP override
	void processTask(SorterOutputTask& task);  // CRTP override
	// void postProcess();  // CRTP override
	void advancePhase(int currentPhase, int newPhase);
	void addCounts(const SorterStatistics& stats)
	{
		stats_ += stats;
	}

private:
	GolBuilder* builder_;
	std::mutex phaseMutex_;
	std::condition_variable phaseStarted_;
	SorterStatistics stats_; 
	double workPerByte_;
	int phaseCountdowns_[3];
};
