// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "XmlQueryPrinter.h"
#include <clarisma/util/log.h>
#include <clarisma/util/Xml.h>
#include <geodesk/feature/MemberIterator.h>
#include <geodesk/feature/Tags.h>
#include <geodesk/feature/WayNodeIterator.h>

using namespace clarisma;

XmlQueryPrinter::XmlQueryPrinter(QuerySpec* spec) :
    SimpleQueryPrinter(spec),
    wayNodeIds_(spec->store()->hasWaynodeIds())
{
    setProgressScope(0, 100 - GENERATE_XML_WORK);
}


void XmlQueryPrinter::printFeature(FeaturePtr feature)
{
    // We don't actually print any features, we merely
    // collect them -- printFooter() will sort and print them
    addFeature(feature);
}


void XmlQueryPrinter::addFeature(FeaturePtr feature)     // NOLINT recursive
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

void XmlQueryPrinter::addNode(NodePtr node)
{
    features_[0][node.id() << 1] = {node};
}

void XmlQueryPrinter::addWay(WayPtr way)
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
                    features_[0][(node.id << 1) | 1] = {node.xy};
                }
            }
            else
            {
                addNode(node.feature);
            }
        }
    }
}

void XmlQueryPrinter::addRelation(RelationPtr rel)   // NOLINT recursive
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


void XmlQueryPrinter::printFooter()
{
    Console::get()->setTask("Generating XML...");

    std::vector<SortableFeature> sorted;
    ConsoleWriter out;
    out.blank();
    out << "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<osm version=\"0.6\" generator=\"geodesk gol/2.0.0\" upload=\"never\">\n";
        // TODO: version

    size_t nodeCount = features_[0].size();
    size_t wayCount = features_[1].size();
    size_t relationCount = features_[2].size();
    size_t totalCount = nodeCount + wayCount + relationCount;

    printFeatures(out, sorted, 0, &XmlQueryPrinter::printNode);
    Console::get()->setProgress(static_cast<int>(
        100.0 - GENERATE_XML_WORK + GENERATE_XML_WORK * nodeCount / totalCount));
    printFeatures(out, sorted, 1, &XmlQueryPrinter::printWay);
    Console::get()->setProgress(static_cast<int>(
        100.0 - GENERATE_XML_WORK + GENERATE_XML_WORK * (nodeCount + wayCount)
            / totalCount));
    printFeatures(out, sorted, 2, &XmlQueryPrinter::printRelation);
    out << "</osm>\n";
    Console::get()->setProgress(100);
}

void XmlQueryPrinter::printFeatures(ConsoleWriter& out,
    std::vector<SortableFeature>& sorted, int typeCode,
    PrintMethodPtr method)
{
    assert(sorted.empty());
    sorted.reserve(features_[typeCode].size());
    for (const auto& [id, data] : features_[typeCode])
    {
        sorted.push_back({id, data});
    }
    std::sort(sorted.begin(), sorted.end());
    for (const auto& [id, data] : sorted)
    {
        (this->*method)(out, id, data);
        out.flush();        // TODO: flush in batches
        out.blank();
    }
    sorted.clear();
}

void XmlQueryPrinter::printNode(ConsoleWriter& out, int64_t id, FeatureData data)
{
    Coordinate xy = (id & 1) ? data.xy : data.node.xy();
    out << "  <node id=\"" << (id >> 1) << "\" ";
    printLatLon(out, xy);
    out << " version=\"1\"";
    if (id & 1)
    {
        out << "/>\n";
    }
    else
    {
        if (data.node.tags().isEmpty()) [[unlikely]]
        {
            out << "/>\n";
        }
        else
        {
            out << ">\n";
            printTags(out, data.node);
            out << "  </node>\n";
        }
    }
}

void XmlQueryPrinter::printLatLon(ConsoleWriter& out, Coordinate xy)
{
    out << "lat=\"";
    out.formatDouble(xy.lat(), 7, false);
    out << "\" lon=\"";
    out.formatDouble(xy.lon(), 7, false);
    out << "\"";
}

void XmlQueryPrinter::printWay(ConsoleWriter& out, int64_t id, FeatureData data)
{
    // LOGS << "Printing way/" << id;
    out << "  <way id=\"" << id << "\" version=\"1\">\n";
    WayNodeIterator iter(store_, data.way, false, wayNodeIds_);
    for (;;)
    {
        WayNodeIterator::WayNode node = iter.next();
        if (node.xy.isNull()) break;

        if (node.feature.isNull() && !wayNodeIds_)
        {
            out << "    <nd ";
            printLatLon(out, node.xy);
            out << "/>\n";
        }
        else
        {
            out << "    <nd ref=\"" << node.id << "\"/>\n";
        }
    }
    printTags(out, data.way);
    out << "  </way>\n";
}

void XmlQueryPrinter::printRelation(ConsoleWriter& out, int64_t id, FeatureData data)
{
    out << "  <relation id=\"" << id << "\" version=\"1\">\n";
    MemberIterator iter(store_, data.relation.bodyptr());
    for (;;)
    {
        FeaturePtr member = iter.next();
        if (member.isNull()) break;
        const char* typeName = member.typeName();
        out << "    <member type=\"" << typeName
            << "\" ref=\"" << member.id()
            << "\" role=\"" << iter.currentRole() << "\"/>\n";
    }
    printTags(out, data.relation);
    out << "  </relation>\n";
}

void XmlQueryPrinter::printTags(ConsoleWriter& out, FeaturePtr feature) const
{
    for (Tag tag : Tags(store_, feature))
    {
        out << "    <tag k=\"";
        Xml::writeEscaped(out, tag.key());
        out << "\" v=\"";
        if (tag.value().isStoredNumeric())  [[unlikely]]
        {
            out << Decimal(tag.value());
        }
        else
        {
            Xml::writeEscaped(out, tag.value().storedString());
        }
        out << "\"/>\n";
    }
}

