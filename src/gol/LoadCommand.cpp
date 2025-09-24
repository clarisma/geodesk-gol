// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "LoadCommand.h"
#include "GolCommand.h"
#include <clarisma/cli/CliApplication.h>
#include <clarisma/cli/CliHelp.h>
#include <clarisma/io/FilePath.h>

#include "gol/load/TileLoader.h"
#include <geodesk/feature/FeatureStore.h>

LoadCommand::LoadCommand()
{
	openMode_ = FeatureStore::OpenMode::WRITE | FeatureStore::OpenMode::CREATE
		| FeatureStore::OpenMode::EXCLUSIVE;
		// TODO: concurrent mode
}

bool LoadCommand::setParam(int number, std::string_view value)
{
	if(GolCommand::setParam(number, value)) return true;
	tesFileNames_.emplace_back(FilePath::withDefaultExtension(value, ".tes"));
	return true;
}

int LoadCommand::setOption(std::string_view name, std::string_view value)
{
	// TODO
	return GolCommand::setOption(name, value);
}

int LoadCommand::run(char* argv[])
{
	int res = GolCommand::run(argv);
	if (res != 0) return res;

	if (tesFileNames_.empty())
	{
		tesFileNames_.emplace_back(FilePath::withExtension(golPath_, ".tes"));
	}
	
	TileLoader loader(&store_, threadCount());
	int tileCount = loader.prepareLoad(tesFileNames_[0].data());

	// TODO: must verify GUID!!!!

	// Caution: prepareLoad walks the tileIndex; if reading multiple TES,
	// need to commit xaction (or look up tile entries via uncommitted
	// blocks)

	if (tileCount == 0)
	{
		Console::end().success() << "All tiles already loaded.\n";
		return 0;
	}
	// TODO: handle transactions here?
	//  If we skip a file because all tiles are loaded, xaction
	//  remains open

	ConsoleWriter().blank() << "Loading "
		<< Console::FAINT_LIGHT_BLUE << FormattedLong(tileCount)
		<< Console::DEFAULT << (tileCount == 1 ? " tile into " : " tiles into ")
		<< Console::FAINT_LIGHT_BLUE << golPath_
		<< Console::DEFAULT << " from "
		<< Console::FAINT_LIGHT_BLUE << tesFileNames_[0].data()
		<< Console::DEFAULT << ":\n";
	loader.load();

	return 0;
}


void LoadCommand::help()
{
	CliHelp help;
	help.command("gol load <gol-file> [<tes-file>] [<options>]",
		"Load tiles from a Tile Element Set.");
	areaOptions(help);
	generalOptions(help);
}
