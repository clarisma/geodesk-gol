// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "LoadCommand.h"
#include "GolCommand.h"
#include <clarisma/cli/CliApplication.h>
#include <clarisma/cli/CliHelp.h>
#include <clarisma/io/FilePath.h>

#include "gol/load/TileLoader.h"
#include <geodesk/feature/FeatureStore.h>

LoadCommand::Option LoadCommand::OPTIONS[] =
{
	{ "w",				OPTION_METHOD(&LoadCommand::setWaynodeIds) },
	{ "waynode-ids",	OPTION_METHOD(&LoadCommand::setWaynodeIds) }
};

LoadCommand::LoadCommand()
{
	addOptions(OPTIONS, sizeof(OPTIONS) / sizeof(Option));
	openMode_ = DO_NOT_OPEN;
	// TileLoader open/creates the GOL
}

bool LoadCommand::setParam(int number, std::string_view value)
{
	if(GolCommand::setParam(number, value)) return true;
	tesFileNames_.emplace_back(FilePath::withDefaultExtension(value, ".gob"));
	return true;
}

int LoadCommand::run(char* argv[])
{
	int res = GolCommand::run(argv);
	if (res != 0) return res;

	if (tesFileNames_.empty())
	{
		tesFileNames_.emplace_back(FilePath::withExtension(golPath_, ".gob"));
	}
	
	TileLoader loader(&store_, threadCount());
	loader.load(golPath_.c_str(), tesFileNames_[0].c_str(), waynodeIds_);
	return 0;
}


void LoadCommand::help()
{
	CliHelp help;
	help.command("gol load <gol-file> [<gob-file>] [<options>]",
		"Load tiles from a Geo-Object Bundle.");
	areaOptions(help);
	generalOptions(help);
}
