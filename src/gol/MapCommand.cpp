// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "MapCommand.h"
#include <clarisma/cli/CliHelp.h>
#include <clarisma/io/FilePath.h>
#include <geodesk/format/LeafletFormatter.h>
#include <geodesk/query/Query.h>
#include "gol/query/MapQueryPrinter.h"

using namespace clarisma;
using namespace geodesk;


MapCommand::Option MapCommand::QUERY_OPTIONS[] =
{
    { "attribution",	    	OPTION_METHOD(&MapCommand::setAttribution) },
    { "A",	            	OPTION_METHOD(&MapCommand::setAttribution) },
    { "edit",	    	OPTION_METHOD(&MapCommand::setEdit) },
    { "e",	            	OPTION_METHOD(&MapCommand::setEdit) },
    { "keys",				OPTION_METHOD(&MapCommand::setKeys) },
    { "k",	    			OPTION_METHOD(&MapCommand::setKeys) },
    { "link",	    	OPTION_METHOD(&MapCommand::setLink) },
    { "l",	            	OPTION_METHOD(&MapCommand::setLink) },
    { "map",	    	OPTION_METHOD(&MapCommand::setMap) },
    { "m",	            	OPTION_METHOD(&MapCommand::setMap) },
    { "popup",	    	OPTION_METHOD(&MapCommand::setPopup) },
    { "p",	            	OPTION_METHOD(&MapCommand::setPopup) },
    { "tooltip",	    	OPTION_METHOD(&MapCommand::setTooltip) },
    { "t",	            	OPTION_METHOD(&MapCommand::setTooltip) },
};

MapCommand::MapCommand()
{
    addOptions(QUERY_OPTIONS, sizeof(QUERY_OPTIONS) / sizeof(Option));
}

const CharSchema MapCommand::VALID_COLOR_CHAR
{
    0b0000011111111111000000000000000000000000000000000000000000000000,
    0b0000011111111111111111111111111010000111111111111111111111111110,
    0b1111111111111111111111111111111111111111111111111111111111111111,
    0b1111111111111111111111111111111111111111111111111111111111111111,
};

bool MapCommand::setParam(int number, std::string_view value)
{
    if (number >= 2)
    {
        const char* start = value.data();
        const char* end = start + value.size();
        const char* p = start;
        Layer* layer = &layers_[layerCount_];
        while (p < end)
        {
            char ch = *p;
            if (ch == ':')
            {
                if (!layer->query.empty())
                {
                    layerCount_++;
                    layer++;
                }
                layer->color = {start,p};
                start = p+1;
                while (start < end)
                {
                    if (*start > 32) break;
                    start++;
                }
                break;
            }
            if (!VALID_COLOR_CHAR.test(ch)) break;
            p++;
        }
        if (!layer->query.empty()) layer->query += ' ';
        layer->query += {start,end};
        return true;
    }
    return GolCommand::setParam(number, value);
}

int MapCommand::setAttribution(std::string_view s)
{
    attribution_ = s;
    return 1;
}

int MapCommand::setEdit(std::string_view s)
{
    featureOptions_.hasEdit = true;
    if (s.empty()) return 0;
    featureOptions_.editUrl = TextTemplate::compile(s);
    return 1;
}

int MapCommand::setKeys(std::string_view s)
{
    keys_ = s;
    return 1;
}

int MapCommand::setLink(std::string_view s)
{
    featureOptions_.hasLink = true;
    if (s.empty()) return 0;
    featureOptions_.linkUrl = TextTemplate::compile(s);
    return 1;
}

int MapCommand::setMap(std::string_view s)
{
    basemapUrl_ = s;
    return 1;
}

int MapCommand::setPopup(std::string_view s)
{
    featureOptions_.hasPopup = true;
    if (s.empty()) return 0;
    featureOptions_.popup = TextTemplate::compile(s);
    return 1;
}

int MapCommand::setTooltip(std::string_view s)
{
    featureOptions_.hasTooltip = true;
    if (s.empty()) return 0;
    featureOptions_.tooltip = TextTemplate::compile(s);
    return 1;
}

int MapCommand::run(char* argv[])
{
    int res = GolCommand::run(argv);
    if (res != 0) return res;

    bool hasLayers = !layers_[0].query.empty();
    if (hasLayers)
    {
        layerCount_++;
        for (int i=0; i<layerCount_; i++)
        {
            layers_[i].matcher = store_.getMatcher(layers_[i].query.c_str());
        }
    }

    FileBuffer2 out;
    Box bounds;
    LeafletFormatter leaflet;
    std::string mapPath = FilePath::withExtension(golPath_, "-temp-map.html");
    out.open(mapPath.c_str());

    LeafletSettings settings;
    settings.attribution = attribution_;
    settings.basemapUrl = basemapUrl_;
    leaflet.writeHeader(out, settings, hasLayers ?
        ".leaflet-popup-content-wrapper {border-radius: 0; padding: 6px 0px 0px 0px; max-height: 90vh; background-color: #f0f0ff; }\n"
        ".leaflet-popup-content a {text-decoration: none; color: darkblue;}\n"
        "a.edit {background-color: darkblue; color: white; font-size: 55%;"
            "text-decoration: none; padding: 2px 6px 2px 6px; border-radius: 5px;"
            "margin-left: 12px; margin-right: 20px; vertical-align: 3px;}\n"
        ".leaflet-popup-content {padding:0px; margin: 0px;}\n"
        ".leaflet-popup-content h3 { background-color: #f0f0ff; margin: 0px; padding: 0px 6px 2px 6px; font-size: 1.75em; }\n"
        ".leaflet-popup-content pre { background-color: #fff; margin: 0; padding: 6px 8px 8px 8px; max-height: 60vh; overflow-y: auto;}"
        ".logo { position: absolute; top: 10px; left: 10px; width: 40px; height: 40px; "
            "background: url('https://www.geodesk.com/images/logo2s.png') no-repeat center center; "
            "background-size: contain; z-index: 500; }\n"
    :
        ".leaflet-interactive:hover {"
            "stroke-dasharray: 2, 2;"
            "stroke: #333;"
            "stroke-width: 1px;"
            "fill: rgba(51, 51, 51, 0.8);}");

    if(!hasLayers)
    {
        // Create tile map

        TileIndexWalker tiw(store().tileIndex(), store().zoomLevels(), Box::ofWorld(), nullptr);
        do
        {
            if(!tiw.currentEntry().isLoadedAndCurrent())
            {
                leaflet.writeBox(out, tiw.currentTile().bounds());
                out << ", {fillColor:\"#333\", weight: 0, fillOpacity: 0.65}).bindTooltip('";
                out << tiw.currentTile();
                out << "<br><b>missing</b>', {direction: 'top'}).addTo(map);";
                tiw.skipChildren();
            }
        }
        while (tiw.next());
    }
    else
    {
        // TODO

        Console::get()->start("Running query...");
        int64_t count = 0;
        for (int i=0; i<layerCount_; i++)
        {
            const MatcherHolder* matcher = layers_[i].matcher;
            QuerySpec spec(&store_, bounds_, matcher->acceptedTypes(),
                    matcher, filter_.get(), 6, keys_);

            std::string_view color = layers_[i].color;
            if (!color.empty())
            {
                leaflet.writeSetColor(out, layers_[i].color);
            }
            MapQueryPrinter printer(out, &spec, &featureOptions_);
            count += printer.run();
            bounds.expandToIncludeSimple(printer.resultBounds());
        }

        Console::end().success() << "Mapped "
            << Console::FAINT_LIGHT_BLUE
            << FormattedLong(count) << Console::DEFAULT
            << (count==1 ? " feature.\n" : " features.\n");
    }

    leaflet.writeFooter(out, bounds);
    out.flush();
    out.close();

#if defined(_WIN32)
    // Windows: Use "start" command
    std::string command = "start " + mapPath;
#elif defined(__linux__)
    // macOS: Use "open" command
    std::string command = "xdg-open " + mapPath + " > /dev/null 2>&1 &";
#elif defined(__APPLE__)
    // macOS: Use "open" command
    std::string command = "open " + mapPath + " > /dev/null 2>&1 &";
#else
#error "Unsupported platform"
#endif
    system(command.c_str());

    return 0;
}

void MapCommand::help()
{
    CliHelp help;
    help.command("gol map <gol-file> [<query>] [<options>]",
        "Display query results on a map.");
    help.beginSection("Output Options:");
    help.option("-m, --map <url>", "Custom base map");
    help.option("-A, --attribution <text>", "Map attribution");
    help.option("-l, --link [<url>]", "Open website on click (default: OpenStreetMap)");
    help.option("-e, --edit [<url>]", "Open editor on click (default: iD)");
    help.option("-p, --popup [<template>]", "Show details on click");
    help.option("-t, --tooltip [<template>]", "Show details on hover");
    help.option("-k, --keys <list>", "Restrict tags to the given keys");
    help.endSection();
    areaOptions(help);
    generalOptions(help);
}