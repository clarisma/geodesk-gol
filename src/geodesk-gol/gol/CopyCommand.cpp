// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "CopyCommand.h"
#include <clarisma/cli/CliApplication.h>
#include <clarisma/cli/CliHelp.h>
#include <clarisma/io/FilePath.h>

#include "gol/load/TileLoader.h"
#include <geodesk/feature/FeatureStore.h>

CopyCommand::CopyCommand()
{
}

bool CopyCommand::setParam(int number, std::string_view value)
{
	if(GolCommand::setParam(number, value)) return true;
	return true;
}

int CopyCommand::setOption(std::string_view name, std::string_view value)
{
	// TODO
	return GolCommand::setOption(name, value);
}

int CopyCommand::run(char* argv[])
{
	int res = GolCommand::run(argv);
	if (res != 0) return res;

	return 0;
}


void CopyCommand::help()
{
	CliHelp help;
	help.command("gol copy <source-gol> <target-gol> [<options>]",
		"Copy tiles from one GOL to another.");
	areaOptions(help);
	generalOptions(help);
}
