// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/alloc/ArenaBag.h>
#include "CFeature.h"

using namespace geodesk;

class ChangedTile 
{
public:
    explicit ChangedTile(Arena& arena, Tip tip) :
        tip_(tip),
        mayGainTex_(arena) {}

    Tip tip() const { return tip_; }
    LinkedStack<ChangedNode>& changedNodes() { return changedNodes_; };
    LinkedStack<ChangedFeatureStub>& changedWays() { return changedWays_; };
    LinkedStack<ChangedFeatureStub>& changedRelations() { return changedRelations_; };
    LinkedStack<ChangedFeatureStub>& deletedNodes() { return deletedNodes_; };
    LinkedStack<ChangedFeatureStub>& deletedWays() { return deletedWays_; };
    LinkedStack<ChangedFeatureStub>& deletedRelations() { return deletedRelations_; };

    void addChanged(ChangedFeatureStub* feature)
    {
        assert(feature->get()->ref().tip() == tip_ ||
            feature->get()->refSE().tip() == tip_);

        if (feature->typedId() == TypedFeatureId::ofWay(89253924))
        {
            LOGS << "Assigning " << feature->typedId() << " to tile " << tip_;
        }
        assert(feature->type() != FeatureType::NODE);
        ((feature->type() == FeatureType::WAY) ? changedWays_ : changedRelations_).push(feature);
    }

    void mayGainTex(CFeatureStub* feature)
    {
        assert(feature);
        mayGainTex_.add(feature);
        hasTexChanges_ = true;

        // TODO: Unless the feature is marked may_have_tex,
        //  it will definitely need a TEX, hence we could
        //  flag the tile differently to avoid scanning
        //  its exported features to see if any in mayGainTex_
        //  already have a TEX
    }

    bool hasTexChanges() const { return hasTexChanges_; }

    const ArenaBag<CFeatureStub*,16>& mayGainTex() const
    {
        return mayGainTex_;
    }

private:
    LinkedStack<ChangedNode> changedNodes_;
    LinkedStack<ChangedFeatureStub> changedWays_;
    LinkedStack<ChangedFeatureStub> changedRelations_;
    LinkedStack<ChangedFeatureStub> deletedNodes_;
    LinkedStack<ChangedFeatureStub> deletedWays_;
    LinkedStack<ChangedFeatureStub> deletedRelations_;
    ArenaBag<CFeatureStub*,16> mayGainTex_;
    Tip tip_;
    bool hasTexChanges_ = false;
};
