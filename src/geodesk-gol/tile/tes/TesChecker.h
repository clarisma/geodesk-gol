// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>
#include <clarisma/data/HashSet.h>
#include <clarisma/util/StreamWriter.h>
#include <clarisma/validate/BinaryChecker.h>
#include <geodesk/feature/FeatureType.h>
#include <geodesk/feature/TagTablePtr.h>
#include <geodesk/feature/Tex.h>
#include <geodesk/feature/Tip.h>
#include <geodesk/feature/TypedFeatureId.h>
#include <geodesk/geom/Tile.h>

using namespace geodesk;
using clarisma::ShortVarString;

class TesChecker : public clarisma::BinaryChecker
{
public:
    TesChecker(Tip tip, Tile tile, const uint8_t* start, size_t size) :
        BinaryChecker(start, size),
        tip_(tip),
        tile_(tile) {}

    struct Feature
    {
        TypedFeatureId typedId;
        const uint8_t* data;
    };

    Tip tip() const { return tip_; }

    void dump(const std::filesystem::path& root);

    template <typename Range>
    static void createFolders(const std::filesystem::path& root, const Range& tips)
    {
        clarisma::HashSet<uint32_t> foldersCreated;
        for(Tip tip: tips)
        {
            uint32_t tipPrefix = tip >> 12;
            if (foldersCreated.find(tipPrefix) == foldersCreated.end())
            {
                char buf[16];
                clarisma::Format::hexUpper(buf, tipPrefix, 3);
                std::filesystem::create_directories(root / buf);
                foldersCreated.insert(tipPrefix);
            }
        }
    }

protected:
    void read();
    void dumpErrors();

private:
    void readFeatureIndex();
    void readStrings();
    void readTagTables();
    void readRelationTables();
    void readChangedFeatures();
    void readRemovedFeatures();
    void readExports();
    const uint8_t* readTagTable(int number);
    const uint8_t* readRelationTable(int number);
    void readRelationTableContents(int number, uint32_t size);
    uint32_t readGlobalTag(uint32_t prevGlobalTag);
    uint32_t readLocalTag();
    uint32_t readTagValue(TagValueType type);
    int readFeatureStub();
    void readNode();
    void readWay();
    void readRelation();
    void checkLocalString(const char* type, uint32_t code);

    void writeLocalString(uint32_t code);
    void writeLocalFeatureRef(uint32_t local);
    void writeForeignFeatureRef(Tip tip, Tex tex);

protected:
    clarisma::StreamWriter out_;
    Tip tip_;
    Tile tile_;
    Box tileBounds_;
    std::vector<Feature> features_;
    std::vector<const ShortVarString*> strings_;
    std::vector<const uint8_t*> tagTables_;
    std::vector<const uint8_t*> relationTables_;
    int featureCounts_[3] = {};
    int changedFeatureCount_ = 0;
    Coordinate prevXY_;
    std::vector<Coordinate> coords_;
};
