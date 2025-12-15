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
    class FeatureData
    {
    public:
        FeatureData() : data_(0) {}

        FeatureData(FeaturePtr f)       // NOLINT implicit
        {
            data_ = reinterpret_cast<uintptr_t>(f.ptr().ptr());
        }

        FeatureData(int32_t lon, int32_t lat)
        {
            data_ = (static_cast<uintptr_t>(lat) << 33) |
                (static_cast<uintptr_t>(static_cast<uint32_t>(lon)) << 1) | 1;
                // need to cast to uint32 first to avoid sign extension
        }

        FeaturePtr feature() const
        {
            assert(isFeature());
            return FeaturePtr(reinterpret_cast<const uint8_t*>(data_));
        }

        NodePtr node() const { return NodePtr(feature()); }
        WayPtr way() const { return WayPtr(feature()); }
        RelationPtr relation() const { return RelationPtr(feature()); }

        int32_t lon() const
        {
            assert(isCoordinate());
            return static_cast<int32_t>(static_cast<int64_t>(data_) >> 1);
        }

        int32_t lat() const
        {
            assert(isCoordinate());
            return static_cast<int32_t>(data_ >> 33);
        }

        bool isFeature() const { return !isCoordinate(); }
        bool isCoordinate() const { return data_ & 1; }

    private:
        uintptr_t data_;
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
    void printFeatures(int typeCode, double startPercentage, double workPerFeature);
    void prepareFeatures(int typeCode);

    static constexpr double FORMATTING_WORK = 20;
        // TODO: calculate better

    clarisma::HashMap<int64_t, FeatureData> features_[3];
    std::vector<SortedFeature> sorted_;
    bool wayNodeIds_;
    int formattingWork_;
};
