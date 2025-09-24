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

class XmlQueryPrinter : public SimpleQueryPrinter
{
public:
    explicit XmlQueryPrinter(QuerySpec* spec);

protected:
    void printFeature(FeaturePtr feature) override;
    void printFooter() override;

private:
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

    struct SortableFeature
    {
        int64_t id;
        FeatureData data;

        bool operator<(const SortableFeature& other) const noexcept
        {
            return id < other.id;
        }
    };

    using PrintMethodPtr = void (XmlQueryPrinter::*)(clarisma::ConsoleWriter&, int64_t, FeatureData);

    void addFeature(FeaturePtr feature);
    void addNode(NodePtr node);
    void addWay(WayPtr way);
    void addRelation(RelationPtr rel);
    void printFeatures(clarisma::ConsoleWriter& out,
        std::vector<SortableFeature>& sorted, int typeCode,
        PrintMethodPtr method);
    void printNode(clarisma::ConsoleWriter& out, int64_t id, FeatureData data);
    void printWay(clarisma::ConsoleWriter& out, int64_t id, FeatureData data);
    void printRelation(clarisma::ConsoleWriter& out, int64_t id, FeatureData data);
    void printLatLon(clarisma::ConsoleWriter& out, Coordinate xy);
    void printTags(clarisma::ConsoleWriter& out, FeaturePtr feature) const;

    static constexpr double GENERATE_XML_WORK = 20;

    clarisma::HashMap<int64_t, FeatureData> features_[3];
    bool wayNodeIds_;
};
