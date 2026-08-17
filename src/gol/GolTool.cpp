// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "GolTool.h"
#include <unordered_map>
#include <clarisma/cli/CliHelp.h>
#include "BuildCommand.h"
#include "CheckCommand.h"
#include "CopyCommand.h"
#include "DefaultCommand.h"
#include "GetCommand.h"
#include "InfoCommand.h"
#include "InstallCommand.h"
#include "DumpTilesCommand.h"
#include "LoadCommand.h"
#include "MapCommand.h"
#include "QueryCommand.h"
#include "SaveCommand.h"
#include "GetIdCommand.h"
#include "UpdateCommand.h"
#include "TestCommand.h"

using namespace clarisma;


int GolTool::run(char* argv[])
{
	std::unordered_map<std::string_view, int(*)(char* argv[])> commandMap =
	{
		{ "build", &GolTool::build },
  		{ "check", &GolTool::check },
#ifdef GOL_EXPERIMENTAL
		{ "copy", &GolTool::copy },
#endif
#ifdef GOL_DIAGNOSTICS
		{ "dump-tiles", &GolTool::dumpTiles },
		{ "test", &GolTool::test },
#endif
#ifdef GOL_EXPERIMENTAL
		{ "get", &GolTool::get },
#endif
		{ "get-id", &GolTool::getId },
		{ "info", &GolTool::info },
		{ "install", &GolTool::install },
		{ "query", &GolTool::query },
		{ "load", &GolTool::load },
		{ "map", &GolTool::map },
		{ "save", &GolTool::save },
#ifdef GOL_EXPERIMENTAL
		{ "update", &GolTool::update }
#endif
	};

	try
	{
		std::string_view cmd = CliCommand::getCommand(argv);
		if(cmd.empty())
		{
			return DefaultCommand().run(argv);
		}

		auto it = commandMap.find(cmd);
		if (it != commandMap.end())
		{
			return (it->second)(argv);
		}
		Console::end().failed() << "Unknown command: "
			<< Console::HIGHLIGHT_YELLOW << cmd
			<< Console::DEFAULT;
	}
	catch (const std::exception& ex)
	{
		fail(ex.what());
	}
	return 1;
}

int GolTool::build(char* argv[])
{
	return BuildCommand().run(argv);
}

int GolTool::check(char* argv[])
{
	return CheckCommand().run(argv);
}

#ifdef GOL_EXPERIMENTAL
int GolTool::copy(char* argv[])
{
	return CopyCommand().run(argv);
}
#endif

#ifdef GOL_DIAGNOSTICS
int GolTool::dumpTiles(char* argv[])
{
	return DumpTilesCommand().run(argv);
}

int GolTool::test(char* argv[])
{
	return TestCommand().run(argv);
}
#endif

#ifdef GOL_EXPERIMENTAL
int GolTool::get(char* argv[])
{
	return GetCommand().run(argv);
}
#endif

int GolTool::info(char* argv[])
{
	return InfoCommand().run(argv);
}

int GolTool::install(char* argv[])
{
	return InstallCommand().run(argv);
}

int GolTool::load(char* argv[])
{
	return LoadCommand().run(argv);
}

int GolTool::map(char* argv[])
{
	return MapCommand().run(argv);
}

int GolTool::query(char* argv[])
{
	return QueryCommand().run(argv);
}

int GolTool::save(char* argv[])
{
	return SaveCommand().run(argv);
}

int GolTool::getId(char* argv[])
{
	return GetIdCommand().run(argv);
}

#ifdef GOL_EXPERIMENTAL
int GolTool::update(char* argv[])
{
	return UpdateCommand().run(argv);
}

#endif

/*
void GolTool::help()
{
	CliHelp help;
	help.command("gol [-V|--version] [-h|--help] <command> [<options>]",
		"Build, manage and query Geographic Object Libraries.");
	help.beginSection("Commands:\n");
	help.subCommand("build", "Create a GOL from an OSM data file");
	help.subCommand("query", "Perform a GOQL query");
	help.subCommand("info", "Obtain metadata and statistics");
	help.subCommand("load", "Load tiles into a GOL");
	help.subCommand("save", "Export tiles from a GOL");
	help.subCommand("check", "Verify integrity");
	help << "\nUse " << Console::WHITE << "gol help "
		<< Console::FAINT_LIGHT_BLUE << "<command>" << Console::DEFAULT
		<< " for detailed documentation.\n\n";
}
*/