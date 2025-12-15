// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "OsmQueryPrinter.h"
#include <geodesk/feature/MemberIterator.h>
#include <geodesk/feature/Tags.h>
#include <geodesk/feature/WayNodeIterator.h>

using namespace clarisma;

OsmQueryPrinter::OsmQueryPrinter(QuerySpec* spec) :
    SimpleQueryPrinter(spec),
    wayNodeIds_(spec->store()->hasWaynodeIds())
{
    double areaRatio = spec->box().area() / Box::ofWorld().area();
    static constexpr double QUERY_PERCENTAGE_MIN = 40;
    static constexpr double QUERY_PERCENTAGE_MAX = 80;
    int queryWork = static_cast<int>(QUERY_PERCENTAGE_MIN +
        (QUERY_PERCENTAGE_MAX - QUERY_PERCENTAGE_MIN) * areaRatio);
    formattingWork_ = 100 - queryWork;
    setProgressScope(0, queryWork);
}


void OsmQueryPrinter::printFeature(FeaturePtr feature)
{
    // We don't actually print any features, we merely
    // collect them -- printFooter() will sort and print them
    addFeature(feature);
}


void OsmQueryPrinter::addFeature(FeaturePtr feature)     // NOLINT recursive
{
    if(feature.isNode())
    {
        addNode(NodePtr(feature));
    }
    else if(feature.isWay())
    {
        addWay(WayPtr(feature));
    }
    else
    {
        addRelation(RelationPtr(feature));
    }
}

void OsmQueryPrinter::addNode(NodePtr node)
{
    features_[0][node.id()] = node;
}

void OsmQueryPrinter::addWay(WayPtr way)
{
    auto [it, inserted] = features_[1].insert(
        {static_cast<int64_t>(way.id()), {way}});

    if (inserted)
    {
        WayNodeIterator iter(store_, way, false, wayNodeIds_);
        for (;;)
        {
            WayNodeIterator::WayNode node = iter.next();
            if (node.xy.isNull()) break;
            if (node.feature.isNull())
            {
                if (wayNodeIds_)
                {
                    features_[0][node.id] = FeatureData(
                        Mercator::lon100ndFromX(node.xy.x),
                        Mercator::lat100ndFromY(node.xy.y));
                }
            }
            else
            {
                addNode(node.feature);
            }
        }
    }
}

void OsmQueryPrinter::addRelation(RelationPtr rel)   // NOLINT recursive
{
    auto [it, inserted] = features_[2].insert(
        {static_cast<int64_t>(rel.id()), {rel}});
    if (inserted)
    {
        MemberIterator iter(store_, rel.bodyptr());
        for (;;)
        {
            FeaturePtr member = iter.next();
            if (member.isNull()) break;
            addFeature(member);
        }
    }
}


void OsmQueryPrinter::printFooter()
{
    size_t nodeCount = features_[0].size();
    size_t wayCount = features_[1].size();
    size_t relCount = features_[2].size();

    // formatting work effort per way/relation relative to node
    static constexpr double WAY_WORK_RATIO = 5;
    static constexpr double REL_WORK_RATIO = 16;

    double totalWayUnits = WAY_WORK_RATIO * wayCount;
    double totalRelUnits = REL_WORK_RATIO * relCount;
    double totalUnits = totalWayUnits + totalRelUnits + nodeCount;
    double totalFormattingWork = formattingWork_;
    double perNodeWork = totalFormattingWork / totalUnits;
    double perWayWork = totalFormattingWork * WAY_WORK_RATIO / totalUnits;
    double perRelWork = totalFormattingWork * REL_WORK_RATIO / totalUnits;

    /*
    if (!features_[0].empty())
    {
        prepareFeatures(0);
        printNodes(sorted_);
    }
    if (!features_[1].empty())
    {
        prepareFeatures(1);
        printWays(sorted_);
    }
    if (!features_[2].empty())
    {
        prepareFeatures(2);
        printRelations(sorted_);
    }
    */
    double startPercentage = 100 - formattingWork_;
    printFeatures(0, startPercentage, perNodeWork);
    startPercentage += perNodeWork * nodeCount;
    printFeatures(1, startPercentage, perWayWork);
    startPercentage += perWayWork * wayCount;
    printFeatures(2, startPercentage, perRelWork);
    endFeatures();
    Console::get()->setProgress(100);   // TODO
}

/*
void OsmQueryPrinter::prepareFeatures(int typeCode)
{
    if (features_[typeCode].empty()) return;

    using Method = void (OsmQueryPrinter::*)(std::span<SortedFeature>);

    static constexpr Method METHODS[] =
    {
        &OsmQueryPrinter::printNodes,
        &OsmQueryPrinter::printWays,
        &OsmQueryPrinter::printRelations
    };

    static constexpr const char* TASKS[] =
    {
        "Writing nodes...",
        "Writing ways...",
        "Writing relations..."
    };

    Console::get()->setTask(TASKS[typeCode]);
    sorted_.clear();
    sorted_.reserve(features_[typeCode].size());
    for (const auto& [id, data] : features_[typeCode])
    {
        sorted_.emplace_back(id, data);
    }
    std::sort(sorted_.begin(), sorted_.end());
    beginFeatures(typeCode);
    METHODS[typeCode](sorted_);
}
*/

void OsmQueryPrinter::printFeatures(int typeCode, double startPercentage, double workPerFeature)
{
    if (features_[typeCode].empty()) return;

    using Method = void (OsmQueryPrinter::*)(std::span<SortedFeature>);

    static constexpr Method METHODS[] =
    {
        &OsmQueryPrinter::printNodes,
        &OsmQueryPrinter::printWays,
        &OsmQueryPrinter::printRelations
    };

    static constexpr const char* TASKS[] =
    {
        "Writing nodes...",
        "Writing ways...",
        "Writing relations..."
    };

    Console::get()->setTask(TASKS[typeCode]);
    sorted_.clear();
    sorted_.reserve(features_[typeCode].size());
    for (const auto& [id, data] : features_[typeCode])
    {
        sorted_.emplace_back(id, data);
    }
    std::sort(sorted_.begin(), sorted_.end());
    beginFeatures(typeCode);

    static constexpr size_t MIN_BATCH_SIZE = 16000;
    size_t batchSize = std::max(
        static_cast<size_t>(0.5f / workPerFeature), MIN_BATCH_SIZE);
    size_t start = 0;
    size_t remaining = sorted_.size();
    do
    {
        batchSize = std::min(batchSize, remaining);
        (this->*METHODS[typeCode])({&sorted_[start], batchSize});
        start += batchSize;
        remaining -= batchSize;
        startPercentage += workPerFeature * batchSize;
        Console::get()->setProgress(static_cast<int>(startPercentage));
    }
    while (remaining);
}


