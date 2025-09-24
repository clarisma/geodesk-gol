// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <semaphore>
#include <build/util/TileCatalog.h>
#include <clarisma/thread/TaskEngine.h>
#include <geodesk/feature/Tip.h>
#include "tile/tes/TesArchiveWriter.h"

#include "change/model/ChangeModel.h"
#include "ChangeWriter.h"
#include "TileChangeAnalyzer.h"

using namespace geodesk;

class Updater;

class UpdaterTask
{
public:
	UpdaterTask() {} // TODO: only to satisfy compiler
	UpdaterTask(Tip tip) :
		tip_(tip)
	{}
	UpdaterTask(int entryNumber) :
		entryNumber_(entryNumber)
	{}

	Tip tip() const { return tip_; }
	int entryNumber() const { return entryNumber_; }

private:
	union
	{
		Tip tip_;
		int entryNumber_;
	};
};

class UpdaterWorker
{
public:
	explicit UpdaterWorker(Updater* updater);

	void processTask(UpdaterTask& task);	// CRTP override
	void afterTasks() {}					// CRTP override
	void harvestResults() {}				// CRTP override
	int64_t unchangedTags() const { return analyzer_.unchangedTags(); }
	void applyActions()
	{
		analyzer_.applyActions();
	}

private:
	void analyze(Tip tip);
	void prepareUpdate(Tip tip);
	void applyUpdate(int entryNumber);

	Updater* updater_;
	TileChangeAnalyzer analyzer_;
	ChangeWriter writer_;
};


class Updater : public clarisma::TaskEngine<Updater, UpdaterWorker, UpdaterTask, TileData>
{
public:
	enum class Phase
	{
		SEARCH,
		PREPARE_UPDATE,
		APPLY_UPDATE
	};

	explicit Updater(FeatureStore* store, UpdateSettings& settings);

    void update(std::string_view url, std::span<const char*> files);

	// TODO: Consider encapsulating as UpdateProgressTracker?
	void beginUpdate(
		uint32_t fromRevision, DateTime fromTimestamp,
		uint32_t toRevision, DateTime toTimestamp);
	void setReadingTask(uint32_t revision);
	void reportFileRead(size_t uncompressedSize);

	ChangeModel& model() { return model_; }
	const TileCatalog& tileCatalog() const { return tileCatalog_; }
	FeatureStore* store() const { return model_.store(); }
	Phase phase() const { return phase_; }
	void taskCompleted();
	void processTask(TileData& task);	// CRTP override
	void postProcess();					// CRTP override

	const TesArchiveEntry& tesEntry(int n)const
	{
		return tesArchive_[n];
	}

	const uint8_t* tesData(int n) const
	{
		assert(n >= 0 && n < tesArchive_.header().entryCount);
		assert(tesOffsets_);
		return tesArchive_.dataAtOffset(tesOffsets_[n]);
	}

	const std::filesystem::path& dumpPath() const { return dumpPath_; };


private:
	void startPhase(Phase phase, int taskCount, double workPerUnit);
	void awaitPhaseCompletion() { phaseCompleted_.acquire(); }
	void completed(double work);
	void processChanges();
	void processNodes();
	void processWays();
	void processRelations();
	void assignToTiles(ChangedFeature2D* feature);
	void processNode(ChangedNode* node);
	void processWay(ChangedFeature2D* way);
	int processRelation(ChangedFeature2D* rel);
	void processDeletedFeature(ChangedFeature2D* deleted);
	void processMembershipChanges(ChangedFeatureBase* feature);
	void addDeleted(Tip tip, ChangedFeatureStub* feature);
	void updateBounds(ChangedFeature2D* future, const Box& bounds);
	void updateTiles(ChangedFeature2D* feature, TilePair futureTiles);
	void checkMemberExports(ChangedFeature2D* rel);
	void checkExport(CFeature* feature, bool willBeForeign);
	void mayGainOrLoseTex(CFeature* member, ChangedFeature2D* parent);
	void cascadeNodeCoordinateChange(NodePtr node, Coordinate futureXY);
	void cascadeBoundsChange(FeaturePtr feature, const Box& futureBounds);
	int normalizeRefs(ChangedFeature2D* changed);
	CRef deduceTwinRef(CRef ref) const;
	FeaturePtr getFeature(CFeature* feature) const
	{
		return feature->getFeature(store());
	}

	void readChangeFiles(std::span<const char*> files);
	void prepareUpdate();
	void applyUpdate();

	// TODO: Consider encapsulating as UpdateProgressTracker?
	void calculateWork(int timespanInSeconds);
	static void printRevision(ConsoleWriter& out, const char* leader,
		uint32_t revision, DateTime timestamp, DateTime now);

	ChangeModel model_;
	TileCatalog tileCatalog_;
	std::string updateFileName_;
	TesArchiveWriter archiveWriter_;
	std::atomic<double> workCompleted_;
	double workPerUnit_;
	Phase phase_;
	std::atomic<int> tasksRemaining_;
	std::binary_semaphore phaseCompleted_;
	bool memberSearchCompleted_ = true; // TODO
	uint32_t targetRevision_;
	DateTime targetTimestamp_;
	TesArchive tesArchive_;
	std::unique_ptr<uint64_t[]> tesOffsets_;

	// TODO: Consider encapsulating as UpdateProgressTracker?
	double workReading_;
	double workAnalyzing_;
	double workPreparing_;
	double workApplying_;
	char displayBuffer_[2][32];
	bool useAltDisplay_ = false;
	int changeFileCount_ = 0;

	std::filesystem::path dumpPath_;
};
