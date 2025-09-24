// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/alloc/ReusableBlock.h>
#include <clarisma/thread/TaskEngine.h>
#include <clarisma/util/TaggedPtr.h>
#include <geodesk/geom/Tile.h>
#include "build/util/ProtoGolReader.h"
#include "ExportFileWriter.h"
#include "ExportTableBuilder.h"
#include "ValidatorPileWriter.h"
#include "VArena.h"
#include "VFeatureIndex.h"

class GolBuilder;

class Validator;

class ValidatorTask
{
public:
	ValidatorTask() : data_(0) {}
	explicit ValidatorTask(uint64_t data) : data_(data) {}
	ValidatorTask(Tile tile, int pile) :
		data_(
			(static_cast<uint64_t>(15 - tile.zoom()) << 58) |
			(static_cast<uint64_t>(isOdd(tile)) << 57) |
			(static_cast<uint64_t>(pile) << 32) |
			static_cast<uint32_t>(tile))
	{
		// Console::msg("Created task for %s pile %d", tile.toString().c_str(), pile);
	}

	static bool isOdd(Tile tile)
	{
		return (tile.column() ^ tile.row()) & 1;
	}

	operator uint64_t() const
	{
		return data_;
	}

	Tile tile() const
	{
		return Tile(static_cast<uint32_t>(data_));
	}

	int pile() const
	{
		return static_cast<int>(data_ >> 32) & 0xffffff;	// lower 24 bits
	}

	int batchId() const
	{
		return static_cast<int>(data_ >> 57);
	}

private:
	uint64_t data_;
};

class ValidatorWorker : public ProtoGolReader<ValidatorWorker>
{
public:
	explicit ValidatorWorker(Validator* validator);

	// CRTP Overrides
	void readNodes(const uint8_t*& p)
	{
		useSection(SECTION_LOCAL_NODES);
		ProtoGolReader::readNodes(p);
	}
	void readWays(const uint8_t*& p)
	{
		useSection(SECTION_LOCAL_WAYS);
		ProtoGolReader::readWays(p);
	}
	void readRelations(const uint8_t*& p)
	{
		useSection(SECTION_LOCAL_RELATIONS);
		ProtoGolReader::readRelations(p);
	}
	void readForeignNodes(const uint8_t*& p)
	{
		useSection(SECTION_OTHER);
		ProtoGolReader::readForeignNodes(p);
	}
	void readForeignFeatures(FeatureType type, const uint8_t*& p)
	{
		useSection(SECTION_OTHER);
		ProtoGolReader::readForeignFeatures(type, p);
	}
	void node(uint64_t id, Coordinate xy, ByteSpan tags);
	void way(uint64_t id, ParentTileLocator locator, ByteSpan body);
	void relation(uint64_t id, ParentTileLocator locator, ByteSpan body);
	void membership(uint64_t relId, ParentTileLocator locator, TypedFeatureId typedMemberId);
	void foreignNode(uint64_t id, Coordinate xy, ForeignFeatureRef ref);
	void foreignFeature(FeatureType type, uint64_t id, const Box& bounds, ForeignFeatureRef ref);

	void processTask(ValidatorTask& task);
	void afterTasks() {}
	void harvestResults() {}

private:
	enum
	{
		SECTION_LOCAL_NODES,
		SECTION_LOCAL_WAYS,
		SECTION_LOCAL_RELATIONS,
		SECTION_OTHER
	};

	VLocalBounds* createBounds(VFeature* f)
	{
		VLocalFeature2D* feature = f->asLocalFeature2D();
		assert(!feature->isProcessed());
		uint64_t tentativeTiles = feature->tentativeTiles;
		feature->bounds = arena_.create<VLocalBounds>(tentativeTiles);
		return feature->bounds;
	}

	template <typename T>
	VArena::Iterator<T> iterate(int section)
	{
		return VArena::Iterator<T>(sections_[section], sections_[section + 1]);
	}

	template <typename T>
	VArena::ReverseIterator<T> iterateReverse(int section)
	{
		return VArena::ReverseIterator<T>(sections_[section], sections_[section + 1]);
	}

	uint64_t childExports(VLocalFeature2D* parent) const
	{
		int twinCode = parent->twinCode();
		return twinCode ? (1ULL << twinCode) : 0;
			// export only to a twin (if any) at the current level
	}

	uint64_t childExports(ParentTileLocator locator) const
	{
		int parentTile = locator.zoomDelta() * 5;
		return (1ULL << parentTile) | (1ULL << (parentTile + locator.twinCode()));
			// export to parent tile and its twin (if any)
	}

	void useSection(int section);
	void addToExportTable(VLocalFeature2D* f);

	void processWays();
	void processWay(VLocalFeature2D* way);
	void processRelations();
	void processRelation(VLocalFeature2D* rel);
	void processNodes();
	void exportNodes();
	void exportFeatures(int section);
	// void writeSpecialNodes();

	Validator* validator_;		// keep this order
	VArena arena_;
	VArena::Section sections_[4];
	VFeatureIndex index_;
	ExportTableBuilder exportTable_;
	ValidatorPileWriter pileWriter_;
	// std::vector<TaggedPtr<VLocalNode,1>> specialNodes_;
	Tile currentTile_;
	int currentSection_;
	// bool isOddBatch_;
};

class ValidatorOutputTask 
{
public:
	ValidatorOutputTask() {} // TODO: not needed, only to satisfy compiler
	ValidatorOutputTask(
		int pile,
		PileSet&& piles, Block<ForeignRelationLookup::Entry> foreignRelations) :
		pile_(pile),
		piles_(std::move(piles)), foreignRelations_(std::move(foreignRelations))
	{
	}

	int pile_;
	PileSet piles_;
	Block<ForeignRelationLookup::Entry> foreignRelations_;
};

class Validator : public TaskEngine<Validator, ValidatorWorker, ValidatorTask, ValidatorOutputTask>
{
public:
	explicit Validator(GolBuilder* builder);
	void validate();
	void processTask(ValidatorOutputTask& task);

private:
	void awaitBatchCompletion();

	GolBuilder* builder_;
	double workPerTile_;
	std::mutex countdownMutex_;
	int batchCountdown_;
	std::condition_variable batchCompleted_;
	ExportFileWriter exportsWriter_;

	friend class ValidatorWorker;
};
