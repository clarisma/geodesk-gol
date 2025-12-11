// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/cli/ConsoleWriter.h>
#include <clarisma/data/HashMap.h>
#include <geodesk/feature/NodePtr.h>
#include <geodesk/feature/RelationPtr.h>
#include <geodesk/feature/WayPtr.h>

#include "SimpleQueryPrinter.h"

using namespace geodesk;

class OsmQueryPrinter : public SimpleQueryPrinter
{
public:
    explicit OsmQueryPrinter(QuerySpec* spec);

protected:
    union FeatureData
    {
        FeatureData() {}
        FeatureData(Coordinate xy) : xy(xy) {}
        FeatureData(NodePtr node) : node(node) {}
        FeatureData(WayPtr way) : way(way) {}
        FeatureData(RelationPtr relation) : relation(relation) {}

        FeatureData(const FeatureData& other)
        {
            std::memcpy(this, &other, sizeof(FeatureData));
        }

        FeatureData& operator=(const FeatureData& other)
        {
            std::memcpy(this, &other, sizeof(FeatureData));
            return *this;
        }

        Coordinate xy;
        NodePtr node;
        WayPtr way;
        RelationPtr relation;
    };

    struct SortedFeature
    {
        int64_t id;
        FeatureData data;

        bool operator<(const SortedFeature& other) const noexcept
        {
            return id < other.id;
        }
    };

    virtual void beginFeatures(int typeCode) {}
    virtual void printNodes(std::span<SortedFeature> nodes) = 0;
    virtual void printWays(std::span<SortedFeature> nodes) = 0;
    virtual void printRelations(std::span<SortedFeature> nodes) = 0;
    virtual void endFeatures() {}

private:
    void printFeature(FeaturePtr feature) override;
    void printFooter() override;
    void addFeature(FeaturePtr feature);
    void addNode(NodePtr node);
    void addWay(WayPtr way);
    void addRelation(RelationPtr rel);
    void prepareFeatures(int typeCode);

    static constexpr double FORMATTING_WORK = 20;
        // TODO: calculate better

    clarisma::HashMap<int64_t, FeatureData> features_[3];
    std::vector<SortedFeature> sorted_;
    bool wayNodeIds_;
};
