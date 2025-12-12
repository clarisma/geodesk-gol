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
    setProgressScope(0, 100 - FORMATTING_WORK);
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
    endFeatures();
    Console::get()->setProgress(100);   // TODO
}

void OsmQueryPrinter::prepareFeatures(int typeCode)
{
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
}

