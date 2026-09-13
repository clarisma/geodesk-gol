// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "build/util/TileCatalog.h"
#include "change/model/ChangeModel.h"
#include "ChangeWriter.h"
#include "TileChangeAnalyzer.h"

using namespace geodesk;

class Updater;

class ChangeManager
{
public:
	ChangeManager(FeatureStore* store, UpdateSettings& settings) :
		model_(store, settings),
		tileCatalog_(store)
	{
	}

	void preProcess();
	void process();
	void postProcess();

	ChangeModel& model() { return model_; }
	const TileCatalog& tileCatalog() const { return tileCatalog_; }
	FeatureStore* store() const { return model_.store(); }

	int changedTileCount() const
	{
		return static_cast<int>(changedTiles_.size());
	}

private:
	void processNodes();
	void processWays();
	void preProcessRelations();
	void processRelations();
	ChangedTile* getChangedTile(Tip tip);
	void assignToTiles(ChangedFeature2D* feature);
	void processNode(ChangedNode* node);
	void processPastCoincidentNode(ChangedNode* node, NodePtr pastNode);
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
	CRef getRef(FeaturePtr feature) const;
	int normalizeRefs(CFeature* feature);
	CRef deduceTwinRef(CRef ref) const;
	ChangedNode* findUniqueLocationNode(Tip tip, Coordinate xy);
	void wayNodeFeatureStatusChanged(Coordinate xy, NodePtr node);

	const CTagTable* getExceptionNodeTags(bool duplicate, bool orphan);

	ChangeModel model_;
	TileCatalog tileCatalog_;
	HashMap<Tip,ChangedTile*> changedTiles_;
	HashMap<Coordinate,ChangedNode*> uniqueLocationNodes_;
	bool memberSearchCompleted_ = true; // TODO
	const CTagTable* duplicateNodeTags_ = nullptr;
	const CTagTable* orphanNodeTags_ = nullptr;
	const CTagTable* duplicateOrphanNodeTags_ = nullptr;
};
