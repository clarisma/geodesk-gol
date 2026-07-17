// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "SaveCommand.h"
#include <clarisma/cli/CliHelp.h>
#include <clarisma/io/FilePath.h>
#include <geodesk/query/TileIndexWalker.h>
#include "gol/load/TileSaver.h"


SaveCommand::Option SaveCommand::OPTIONS[] =
{
	{ "w",				OPTION_METHOD(&SaveCommand::setWaynodeIds) },
	{ "waynode-ids",	OPTION_METHOD(&SaveCommand::setWaynodeIds) }
};

SaveCommand::SaveCommand()
{
	addOptions(OPTIONS, sizeof(OPTIONS) / sizeof(Option));
}

bool SaveCommand::setParam(int number, std::string_view value)
{
	if (number == 2)
	{
		gobPath_ = FilePath::withDefaultExtension(value, ".gob");
		return true;
	}
	return GolCommand::setParam(number, value);
}

/*
int GetCommand::setOption(std::string_view name, std::string_view value)
{

}

*/

int SaveCommand::run(char* argv[])
{
	int res = GolCommand::run(argv);
	if (res != 0) return res;

	if (waynodeIds_ && !store_.hasWaynodeIds())
	{
		throw std::runtime_error("Library does not contain waynode IDs");
	}

	if (gobPath_.empty())
	{
		gobPath_ = FilePath::withExtension(golPath_, ".gob");
	}

#ifndef NDEBUG
	std::unordered_set<Tip> tips;
#endif

	std::vector<std::pair<Tile, Tip>> tiles;
	TileIndexWalker tiw(store().tileIndex(), store().zoomLevels(), Box::ofWorld(), filter_.get());
	// TODO: Take box from filter/bbox param
	do
	{
#ifndef NDEBUG
		auto [iter, wasInserted] = tips.insert(tiw.currentTip());
		assert(wasInserted);
#endif
		if(tiw.currentEntry().isLoadedAndCurrent())
		{
			tiles.emplace_back(tiw.currentTile(), tiw.currentTip());
		}
	}
	while (tiw.next());

	ConsoleWriter() << "Saving "
		<< Console::FAINT_LIGHT_BLUE << FormattedLong(tiles.size())
		<< Console::DEFAULT << (tiles.size()==1 ? " tile from " : " tiles from ")
		<< Console::FAINT_LIGHT_BLUE << golPath_
		<< Console::DEFAULT << " to "
		<< Console::FAINT_LIGHT_BLUE << gobPath_
		<< Console::DEFAULT << ":\n";

	TileSaver saver(&store(), threadCount());
	std::string tmpFilePath = gobPath_ + ".tmp";
	saver.save(tmpFilePath.c_str(), tiles, waynodeIds_);
	File::rename(tmpFilePath.c_str(), gobPath_.c_str());
	Console::end().success() << "Done.\n";
	return 0;
}


void SaveCommand::help()
{
    CliHelp help;
    help.command("gol save <gol-file> [<gob-file>] [<options>]",
        "Save a GOL's tiles as a Geo-Object Bundle.");
    // help.option("-M, --omit-metadata", "Omit metadata from GOB\n");
	help.option("-w, --waynode-ids", "Include IDs of all nodes\n");
    areaOptions(help);
    generalOptions(help);
}