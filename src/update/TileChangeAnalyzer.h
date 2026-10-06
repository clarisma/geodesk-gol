// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/alloc/Arena.h>
#include <clarisma/data/HashMap.h>
#include <clarisma/data/HashSet.h>
#include <geodesk/feature/Tex.h>
#include <geodesk/feature/WayNodeIterator.h>
#include "change/model/CFeature.h"
#include "tile/model/TileReaderBase.h"

using namespace geodesk;
using clarisma::HashMap;
using clarisma::HashSet;

class ChangeAction;
class ChangeModel;
class ChangedFeature2D;



class TileChangeAnalyzer : public TileReaderBase<TileChangeAnalyzer>
{
public:
    explicit TileChangeAnalyzer(ChangeModel& model) :
        model_(model),
        pTile_(nullptr),
        actions_(nullptr)
    {
    }

    ChangeModel& model() const { return model_; }

    void analyze(Tip tip, Tile tile, TilePtr pTile);
    void applyActions();
    int64_t unchangedTags() const { return unchangedTags_; }

private:
    struct WayNodeCheckResult
    {
        bool geometryChanged;
        CFeature* node;
    };

    void readNode(NodePtr node);                // CRTP override
    void readWay(WayPtr way);                   // CRTP override
    void readRelation(RelationPtr relation);    // CRTP override
    void readExports();
    int32_t handleOfLocal(FeaturePtr p) const { return p.ptr().ptr() - pTile_.ptr(); }
    CRef refOfLocal(FeaturePtr feature) const;
    CRef refOfWayNode(const WayNodeIterator::WayNode& node) const;
    void compareTags(ChangedFeatureBase* changed, FeaturePtr p);
    void compareWayNodes(ChangedFeature2D* changed, WayPtr way);
    static void compareAreaStatus(ChangedFeature2D* changed, FeaturePtr feature);
    void scanWayNodes(WayPtr way);
    WayNodeCheckResult checkWayNode(const WayNodeIterator::WayNode& node);
    void checkMembers(ChangedFeature2D* changed, RelationPtr relation);
    template<typename Action, typename... Args>
    void addAction(Args&&... args);

    ChangeModel& model_;
    TilePtr pTile_;
    clarisma::Arena arena_;
    HashMap<int32_t,Tex> exports_;
    ChangeAction* actions_;
    HashSet<TypedFeatureId> pastMembers_;
    HashSet<TypedFeatureId> futureMembers_;
    Tip tip_;
    int32_t tileMaxX_;
    int32_t tileMinY_;

    int64_t unchangedTags_ = 0;
    int64_t unchangedMembers_ = 0;

    friend class TileReaderBase;
};
