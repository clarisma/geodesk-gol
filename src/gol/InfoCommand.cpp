// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "InfoCommand.h"
#include <clarisma/cli/CliHelp.h>
#include <clarisma/text/Table.h>
#include <clarisma/util/FileSize.h>
#include <geodesk/query/Query.h>

#include "geodesk/feature/TilePtr.h"

using namespace clarisma;
using namespace geodesk;

InfoCommand::InfoCommand()
{
}

bool InfoCommand::setParam(int number, std::string_view value)
{
    return GolCommand::setParam(number, value);
}

int InfoCommand::setOption(std::string_view name, std::string_view value)
{
    return GolCommand::setOption(name, value);
}

int InfoCommand::run(char* argv[])
{
    int res = GolCommand::run(argv);
    if (res != 0) return res;

    ConsoleWriter out;
    char buf[256];

    DateTime now = DateTime::now();
    Format::timeAgo(buf, (now - store_.revisionTimestamp()) / 1000);

    out << "Tileset ID: " << Console::FAINT_LIGHT_BLUE
        << store_.guid() << Console::DEFAULT
        << "\nTiles:      " << Console::FAINT_LIGHT_BLUE
        << FormattedLong(store_.tileCount()) << Console::DEFAULT
        << "\nSize:       " << Console::FAINT_LIGHT_BLUE
        << FileSize(store_.allocatedSize()) << Console::DEFAULT;
    showRevisionInfo(out);
    out << "Levels:     " << Console::FAINT_LIGHT_BLUE
        << store_.zoomLevels() << Console::DEFAULT << "\n";

    const FeatureStore::Header* header = store_.header();
    out << "            Hilbert-" << header->settings.rtreeBranchSize << "  "
        << static_cast<uint32_t>(header->settings.maxKeyIndexes) << " indexes (min. "
        << header->settings.keyIndexMinFeatures << " features)\n";

    std::vector<std::string_view> indexedKeys = store_.indexedKeyStrings();
    std::ranges::sort(indexedKeys);
    Table table;
    std::vector<Table::Cell> cells;
    cells.reserve(indexedKeys.size());
    for(auto key : indexedKeys)
    {
        cells.emplace_back(key);
    }
    table.distributeColumns(cells, 6, 70);
    table.writeTo(out, 12);

#ifdef GOL_DIAGNOSTICS    
    if (Console::verbosity() >= Console::Verbosity::VERBOSE)
    {
        out.flush();
        printTileStatistics(out);
    }
#endif
    /*
    std::map<std::string_view, std::string_view> properties = store_.properties();
    char buf[100];
    int maxWidth = 0;
    for (auto& entry: properties)
    {
        if(entry.first.size() > maxWidth) maxWidth = entry.first.size();
    }
    */
    return 0;
}

void InfoCommand::showRevisionInfo(ConsoleWriter& out)
{
    char buf[256];

    DateTime now = DateTime::now();
    Format::timeAgo(buf, (now - store_.revisionTimestamp()) / 1000);

    out << "\nRevision:   " << Console::FAINT_LIGHT_BLUE
        << store_.revision() << Console::DEFAULT << " • " << Console::FAINT_LIGHT_BLUE
        << store_.revisionTimestamp() << Console::DEFAULT
        << " (" << buf
        << ")\nUpdatable:  ";

    bool hasWaynodeIDs = store_.hasWaynodeIds();
    if (hasWaynodeIDs)
    {
        out << Console::GREEN << "Yes" << Console::DEFAULT << " (via Osmosis Server)\n";
    }
    else
    {
        out << Console::BRIGHT_ORANGE << "No" << Console::DEFAULT;
        if (!hasWaynodeIDs)
        {
            out << " (built without waynode IDs)\n";
        }
        // TODO: secondary reason: missing/stale tiles
    }

}

void InfoCommand::printTileStatistics(ConsoleWriter& out)
{
    // TODO: restrict based on area, bbox or tileset
    TileIndexWalker tiw(store_.tileIndex(), store_.zoomLevels(), Box::ofWorld(), nullptr);
    do
    {
        if(!tiw.currentEntry().isLoadedAndCurrent()) [[unlikely]]
        {
            continue;
        }
        Tile tile = tiw.currentTile();
        out << tiw.currentTip() << "," << tile << ","
            << tile.zoom() << ","
            << tile.column() << ","
            << tile.row() << ","
            << store_.fetchTile(tiw.currentTip()).totalSize() << "\n";
    }
    while (tiw.next());
}

void InfoCommand::help()
{
    CliHelp help;
    help.command("gol query <gol-file> [<options>]",
        "Obtain information about a GOL.");
    help.beginSection("Output Options:");
    help.option("-o, --output <file>", "Write results to a file");
    help.endSection();
    areaOptions(help);
    generalOptions(help);
}