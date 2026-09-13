// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/data/HashMap.h>
#include <clarisma/data/LinkedStack.h>
#include <clarisma/util/BufferWriter.h>
#include <geodesk/geom/Box.h>
#include "change/model/CTagTable.h"
#include "TableEncoder.h"

using namespace geodesk;
using clarisma::HashMap;
using clarisma::LinkedStack;

namespace clarisma {
class ShortVarString;
}

class ChangeModel;
class ChangedTile;
class CFeature;
class ChangedFeatureStub;
class ChangedFeatureBase;
class ChangedNode;
class ChangedFeature2D;
class CRelationTable;
class TileCatalog;

using clarisma::Buffer;
using clarisma::BufferWriter;
using clarisma::ShortVarString;

class ChangeWriter 
{
public:
    explicit ChangeWriter(const ChangeModel& model, const TileCatalog& tileCatalog) :
        model_(model),
        tileCatalog_(tileCatalog),
        tile_(nullptr) {}

    void write(ChangedTile* tile, Buffer* buf);

private:
    struct SharedItem
    {
        int usage;
        uint32_t code;
        const void* item;

        bool operator<(const SharedItem& other) const
        {
            return usage < other.usage;
        }
    };

    struct ParentRelation
    {
        const CFeature* relation;
        Tip tip;

        ParentRelation(CFeature* relation, Tip tip) :
            relation(relation), tip(tip) {}

        bool operator<(const ParentRelation& other) const
        {
            return tip==other.tip ?
                relation->id() < other.relation->id() :
                tip < other.tip;
        }
    };

    bool isNewToThisTile(const ChangedFeature2D* feature) const;
    void gatherFeatures();
    int gatherRemovedFeatures(const LinkedStack<ChangedFeatureStub>& removed);
    bool addChangedFeature(const ChangedFeatureBase* feature);
    void useTagTable(const CTagTable* tagTable);
    void useRelationTable(const CRelationTable* relTable);
    void prepareFeatures(std::vector<const CFeature*>& featureList, int startingNumber);
    template<typename T>
    void writeSharedItems(HashMap<T*,int>& items, void (ChangeWriter::*write)(T*));
    void writeStrings();
    void writeTagTables();
    void writeRelationTables();
    void writeFeatureIndex();
    void writeFeatureIndex(const std::vector<const CFeature*>& featureList);
    void writeFeatures();
    template <typename T>
    void writeFeatures(FeatureType type, void (ChangeWriter::*write)(T*));
    void writeNode(const ChangedNode* node);
    void writeWay(const ChangedFeature2D* way);
    void writeRelation(const ChangedFeature2D* relation);
    void writeRelationMembers(const ChangedFeature2D* relation);
    int writeStub(const ChangedFeatureBase* feature, int flags, int flagsIfNew);
    void writeBounds(const Box& bounds);
    void writeTagTable(const CTagTable* tags);
    void writeTag(uint32_t keyAndFlags, CTagTable::Tag tag);
    void writeRelationTable(const CRelationTable* relTable);
    void writeRemovedFeatures();
    void writeRemovedFeatures(int start, int count);
    void writeExports();

    const ChangeModel& model_;
    const TileCatalog& tileCatalog_;
    ChangedTile* tile_;
    HashMap<uint32_t,int> strings_;
    HashMap<const CTagTable*,int> tagTables_;
    HashMap<const CRelationTable*,int> relationTables_;
    HashMap<const CFeature*,int> features_;
    std::vector<const CFeature*> featureLists_[3];
    std::vector<const ChangedFeatureBase*> removedFeatures_;
    std::vector<SharedItem> sharedItems_;
    std::vector<uint32_t> table_;
    std::vector<ParentRelation> parentRelations_;
    BufferWriter out_;
    Coordinate tileBottomLeft_;
    Coordinate prevXY_;
};
