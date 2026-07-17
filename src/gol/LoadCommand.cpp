// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "LoadCommand.h"
#include "GolCommand.h"
#include <clarisma/cli/CliApplication.h>
#include <clarisma/cli/CliHelp.h>
#include <clarisma/io/FilePath.h>
#include <clarisma/net/UrlUtils.h>
#include <clarisma/validate/Validate.h>
#include "gol/load/TileLoader.h"
#include <geodesk/feature/FeatureStore.h>


LoadCommand::Option LoadCommand::OPTIONS[] =
{
	{ "C",				OPTION_METHOD(&LoadCommand::setConnections) },
	{ "connections",	OPTION_METHOD(&LoadCommand::setConnections) },
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
	if (number == 0) return true;   // command itself
	if (number > 1)
	{
		if (number > 2 || isRemoteGob_) return false;
		// more than 2 params (or more than 2 URLs) are not allowed
	}

	if (UrlUtils::isUrl(value.data()))	// safe, value is 0-terminated
	{
		gobFileName_ = value;
		isRemoteGob_ = true;
		if (number == 1)
		{
			std::string_view baseName = FilePath::withoutExtension(
				FilePath::name(value));
			if (std::string_view(FilePath::extension(baseName)) == ".osm")
			{
				baseName = FilePath::withoutExtension(baseName);
			}
			golPath_ = FilePath::withExtension(baseName, ".gol");
		}
	}
	else
	{
		if (number == 1)
		{
			golPath_ = FilePath::withDefaultExtension(value, ".gol");
		}
		else
		{
			gobFileName_ = FilePath::withDefaultExtension(value, ".gob");
		}
	}
	return true;
}

int LoadCommand::setConnections(std::string_view s)
{
	connections_ = Validate::intValue(s.data(), MIN_CONNECTIONS, MAX_CONNECTIONS);
	return 1;
}


int LoadCommand::run(char* argv[])
{
	int res = GolCommand::run(argv);
	if (res != 0) return res;

	if (gobFileName_.empty())
	{
		gobFileName_ = FilePath::withExtension(golPath_, ".gob");
	}
	
	TileLoader loader(&store_, threadCount());
	if (isRemoteGob_)
	{
		loader.download(golPath_.c_str(), gobFileName_.c_str(), waynodeIds_,
			bounds_, filter_.get(), connections_);
	}
	else
	{
		loader.load(golPath_.c_str(), gobFileName_.c_str(), waynodeIds_,
			bounds_, filter_.get());
	}
	return 0;
}


void LoadCommand::help()
{
	CliHelp help;
	help.command("gol load [<gol-file>] <gob-file-or-url> [<options>]",
		"Load tiles from a Geo-Object Bundle (local or remote).");

	help.option("-C, --connections", "Max connections when downloading (default: 4)\n");
	help.option("-w, --waynode-ids", "Include IDs of all nodes\n");
	areaOptions(help);
	generalOptions(help);
}
