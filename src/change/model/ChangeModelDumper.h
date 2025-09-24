// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/util/StreamWriter.h>
#include <vector>
#include <clarisma/data/LinkedStack.h>

#include "ChangeFlags.h"
#include "ChangeModel.h"
#include "CTagTable.h"
#include "clarisma/io/FileBuffer3.h"
class ChangedFeatureBase;
class ChangedFeature2D;


class ChangeModelDumper 
{
public:
    ChangeModelDumper(ChangeModel& model) : model_(model) {}
    void dump(const char* fileName);

private:
    void dumpNode(const ChangedNode* node);
    void dumpWay(const ChangedFeature2D* way);
    void dumpRelation(const ChangedFeature2D* rel);
    void dumpFeatureStub(const ChangedFeatureBase* feature);
    void dumpFlags(ChangeFlags flags);
    void dumpTags(const CTagTable* tags);
    void dumpTagValue(CTagTable::Tag tag);
    void dumpParentRelations(const CRelationTable* rels);
    void dumpGlobalString(uint32_t code);
    void dumpLocalString(uint32_t code);
    void dumpBounds(const Box& bounds);
    void dumpTexChanges();

    template<typename T>
    void dumpFeatures(FeatureType type, void (ChangeModelDumper::*dump)(const T*));

    FileBuffer3 out_;
    ChangeModel& model_;
    std::vector<const char*> tempStrings_;
    std::vector<ChangedFeatureBase*> features_;
};
