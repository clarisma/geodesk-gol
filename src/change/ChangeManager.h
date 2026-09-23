// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "build/util/TileCatalog.h"
#include "change/model/ChangeModel.h"
#include "ChangeWriter.h"
#include "TileChangeAnalyzer.h"

using namespace geodesk;

class Updater;

// TODO: Own changed tiles now, need to clear them if the ChangeManager is reused

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
	ChangedTile* getChangedTile(Tip tip);
	const HashMap<Tip,ChangedTile*>& changedTiles() const
	{
		return changedTiles_;
	}
	int changedTileCount() const
	{
		return static_cast<int>(changedTiles_.size());
	}

private:
	struct LinkedRelation
	{
		ChangedFeature2D* relation;
		LinkedRelation* next;
	};

	void processNodes();
	void processWays();
	void preProcessRelations();
	void processRelations();
	void checkParentRelations(LinkedRelation* pRelation, ChangeFlags parentFlags);
	double computeSuperRelationScore(ChangedFeature2D* rel);
	void breakRefcycle(LinkedRelation* pRelation);

	CRef getRef(FeaturePtr feature) const;
	int normalizeRefs(CFeature* feature) const;
	ChangedNode* findUniqueLocationNode(Tip tip, Coordinate xy);
	void wayNodeFeatureStatusChanged(Coordinate xy, NodePtr node);

	const CTagTable* getExceptionNodeTags(bool duplicate, bool orphan);

	static bool isResolved(CFeature* feature)
	{
		if (feature->ref().canGetFeature())  [[likely]]
		{
			CRef refSE = feature->refSE();
			if (feature->type() == FeatureType::NODE) return true;
			if (refSE == CRef::SINGLE_TILE)  [[likely]]
			{
				return true;
			}
			return refSE.canGetFeature();
		}
		return false;
	}

	void ensureResolved(const CRelationTable* rels);
	void remove(ChangedFeatureBase* feature, bool fromSE, bool useOriginal = false);
	void texChange(CFeature* feature, bool inSE, bool texNeeded);
	void confirmTexLoss(ChangedFeatureBase* feature);
	void resolveExports();

	ChangeModel model_;
	TileCatalog tileCatalog_;
	HashMap<Tip,ChangedTile*> changedTiles_;
		// TODO: Make this a plain array? If there are few changed tiles,
		//  the extra memory usage won't matter; if there are many, we
		//  save memory, and lookups are faster in either case
	HashMap<Coordinate,ChangedNode*> uniqueLocationNodes_;
	bool memberSearchCompleted_ = true; // TODO
	const CTagTable* duplicateNodeTags_ = nullptr;
	const CTagTable* orphanNodeTags_ = nullptr;
	const CTagTable* duplicateOrphanNodeTags_ = nullptr;

	friend class FeatureProcessor;
	friend class Feature2dProcessor;
	friend class NodeProcessor;
	friend class WayProcessor;
	friend class RelationProcessor;
};
