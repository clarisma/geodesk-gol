// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "AbstractQueryCommand.h"
#include "gol/map/MapFeatureOptions.h"
#include <clarisma/io/FileBuffer2.h>
#include <clarisma/text/TextTemplate.h>
#include <clarisma/util/Parser.h>

class MapCommand : public GolCommand
{
public:
    MapCommand();

    int run(char* argv[]) override;

private:
    static Option QUERY_OPTIONS[];

    void help() override;
    bool setParam(int number, std::string_view value) override;
    int setAttribution(std::string_view s);
    int setEdit(std::string_view s);
    int setLink(std::string_view s);
    int setKeys(std::string_view s);
    int setMap(std::string_view s);
    int setPopup(std::string_view s);
    int setTooltip(std::string_view s);

    struct Layer
    {
        std::string_view color;
        std::string query;
        const MatcherHolder* matcher = nullptr;
    };

    static constexpr int MAX_LAYERS = 16;
    static const clarisma::CharSchema VALID_COLOR_CHAR;

    Layer layers_[MAX_LAYERS];
    int layerCount_ = 0;
    std::string_view basemapUrl_ = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
    std::string_view attribution_ = "Map data &copy; <a href=\"http://openstreetmap.org\">OpenStreetMap</a> contributors";
    int minZoom_ = 0;
    int maxZoom_ = 19;
    std::string_view keys_;
    MapFeatureOptions featureOptions_;
};

