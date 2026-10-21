// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "GolCommand.h"
#include "gol/query/OutputFormat.h"
#include <geodesk/feature/FeatureType.h>
#include <geodesk/format/KeySchema.h>
#include <vector>
#include <utility>

class GetIdCommand : public GolCommand
{
public:
    GetIdCommand();
    int run(char* argv[]) override;

private:
    static Option OPTIONS[];

    void help() override;
    bool setParam(int number, std::string_view value) override;
    int setFormat(std::string_view s);
    int setKeys(std::string_view s);
    int setPrecision(std::string_view s);

    bool parseTypedId(std::string_view arg,
        geodesk::FeatureType& type, uint64_t& id);

    void printBrief(const std::vector<geodesk::FeaturePtr>& features);
    void printGeoJson(const std::vector<geodesk::FeaturePtr>& features, bool linewise);
    void printWkt(const std::vector<geodesk::FeaturePtr>& features);
    void printCsv(const std::vector<geodesk::FeaturePtr>& features);
    void printList(const std::vector<geodesk::FeaturePtr>& features);

    std::vector<std::pair<geodesk::FeatureType, uint64_t>> ids_;
    OutputFormat format_ = OutputFormat::BRIEF;
    int precision_ = 7;
    std::string_view keys_;
};
