// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "BuildCommand.h"

#include <iterator>
#include <clarisma/cli/CliApplication.h>
#include <clarisma/cli/CliHelp.h>
#include <clarisma/io/FilePath.h>


BuildCommand::Option BuildCommand::BUILD_OPTIONS[] =
{
	{ "areas",				OPTION_METHOD(&BuildCommand::setAreaRules) },
 	{ "i",		OPTION_METHOD(&BuildCommand::setIdIndexing) },
 	{ "id-indexing",		OPTION_METHOD(&BuildCommand::setIdIndexing) },
	{ "indexed-keys",		OPTION_METHOD(&BuildCommand::setIndexedKeys) },
	{ "key-index-min-features", OPTION_METHOD(&BuildCommand::setKeyIndexMinFeatures) },
	{ "l",					OPTION_METHOD(&BuildCommand::setLevels) },
	{ "levels",				OPTION_METHOD(&BuildCommand::setLevels) },
	{ "max-key-indexes",	OPTION_METHOD(&BuildCommand::setMaxKeyIndexes) },
	{ "max-strings",		OPTION_METHOD(&BuildCommand::setMaxStrings) },
	{ "m",					OPTION_METHOD(&BuildCommand::setMaxTiles) },
	{ "max-tiles",			OPTION_METHOD(&BuildCommand::setMaxTiles) },
	{ "min-string-usage",	OPTION_METHOD(&BuildCommand::setMinStringUsage) },
	{ "n",						   OPTION_METHOD(&BuildCommand::setMinTileDensity) },
	{ "min-tile-density",	OPTION_METHOD(&BuildCommand::setMinTileDensity) },
	{ "r",					OPTION_METHOD(&BuildCommand::setRTreeBranchSize) },
	{ "rtree-branch-size",	OPTION_METHOD(&BuildCommand::setRTreeBranchSize) },
	{ "u",					OPTION_METHOD(&BuildCommand::setUpdatable) },
	{ "updatable",			OPTION_METHOD(&BuildCommand::setUpdatable) },
	{ "w",					OPTION_METHOD(&BuildCommand::setWaynodeIds) },
	{ "waynode-ids",		OPTION_METHOD(&BuildCommand::setWaynodeIds) }
};

BuildCommand::BuildCommand()
{
	addOptions(BUILD_OPTIONS, sizeof(BUILD_OPTIONS) / sizeof(Option));
}

bool BuildCommand::setParam(int number, std::string_view value)
{
	switch(number)
	{ 
	case 0:
		return true;
	case 1:
		golPath_ = FilePath::withDefaultExtension(value, ".gol");
		return true;
	case 2:
		sourcePath_ = FilePath::withDefaultExtension(value, ".osm.pbf");
		return true;
	default:
		return false;
	}
}

int BuildCommand::setOption(std::string_view name, std::string_view value)
{
	return BasicCommand::setOption(name, value);
}

int BuildCommand::run(char* argv[])
{
	int res = BasicCommand::run(argv);
	if (res != 0) return res;

	if(golPath_.empty())
	{
		help();
		return 0;
	}

	if(File::exists(golPath_.c_str()) && !yesToAllPrompts_)
	{
		ConsoleWriter out;
		out.arrow() << Console::FAINT_LIGHT_BLUE << FilePath::name(golPath_)
			<< Console::DEFAULT << " exists already. Replace it?";
		if(out.prompt(false) != 1) return 0;
	}


	ConsoleWriter() << "Building "
		<< Console::FAINT_LIGHT_BLUE << FilePath::name(golPath_)
		<< Console::DEFAULT << " from "
		<< Console::FAINT_LIGHT_BLUE << FilePath::name(sourcePath_)
		<< Console::DEFAULT << ":\n";

	BuildSettings& settings = builder_.settings();
	settings.setSource(sourcePath_);
	settings.setThreadCount(threadCount());
	settings.complete();
	builder_.build(golPath_.c_str());

	Console::end().success() << "Done.\n";
	return 0;
}


void BuildCommand::help()
{
	CliHelp help;
	help.command("gol build <gol-file> [<osm-pbf-file>] [<options>]",
		"Builds a GOL from an .osm.pbf source file.");
	help.beginSection("Content Options:");
	help.option("--areas <rules>",
		"Rules to determine if a closed way is considered an area");
	help.option("--max-strings <n>",
		"Maximum number of strings to include in the global string table (256 - 65533, default: 32000)");
	help.option("--min-string-usage <n>",
		"Minimum usage count to consider including a string in the global string table");
	help.option("-w, --waynode-ids",
		"Include IDs of all way-nodes (Increases GOL size by 20%)");
	help.option("-u, --updatable",
		"Enable incremental updates (implies options -w and -i)");
	help.endSection();

	help.beginSection("Tiling Options:");
	help.option("-l, --levels <levels>",
		"Levels of the tile pyramid (default: 0/2/4/6/8/10/12)");
	help.option("-m, --max-tiles <n>",
		"Maximum number of tiles (1 - 8000000, default: 65535)");
	help.option("-n, --min-tile-density <n>",
		"Minimum node count in a tile to avoid consolidation (1 - 10000000, default: 75000)");
	help.endSection();

	help.beginSection("Indexing Options:");
	help.option("-i, --id-indexing",
		"Enable lookups by ID (faster updates, but requires more storage)");
	help.option("--indexed-keys <keys>",
		"Keys to consider for tag-based indexing");
	help.option("--max-key-indexes <n>",
		"Maximum number of key-based sub-indexes "
		"(0 - 30, default: 8)");
	help.option("--key-index-min-features <n>",
		"Minimum number of features in a key index "
		"(1 - 1000000, default: 300)");
	help.option("-r, --rtree-branch-size <n>",
		"Maximum items per R-tree branch (4-256, default: 16)");
	help.endSection();

	generalOptions(help);
}