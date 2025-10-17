// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "DefaultCommand.h"
#include <clarisma/cli/CliHelp.h>

using namespace clarisma;

int DefaultCommand::setOption(std::string_view name, std::string_view value)
{
    int res = BasicCommand::setOption(name, value);
    if(res >= 0) return res;
    if(name == "V" || name == "version")
    {
        showVersion_ =true;
        return 0;
    }
    if(name == "h" || name == "help")
    {
        showHelp_ =true;
        return 0;
    }
    return -1;
}

int DefaultCommand::run(char* argv[])
{
    int res = BasicCommand::run(argv);
    if (res != 0) return res;

    if(showVersion_)
    {
        ConsoleWriter out;
        out << "gol " GEODESK_GOL_VERSION;
        return 0;
    }
    help();
    return 0;
}

void DefaultCommand::help()
{
    CliHelp help;
    help.command("gol [-V|--version] [-h|--help] <command> [<options>]",
        "Build, manage and query Geo-Object Libraries and Bundles.");
    help.beginSection("Commands:\n");
    help.subCommand("build", "Create a GOL from an OSM data file");
    help.subCommand("query", "Perform a GOQL query");
    help.subCommand("map", "Display features on a map");
    help.subCommand("info", "Obtain metadata and statistics");
#ifdef GOL_EXPERIMENTAL
    help.subCommand("load", "Load tiles into a GOL");
    help.subCommand("save", "Export tiles from a GOL");
#endif
    help.subCommand("check", "Verify integrity");
    help << "\nUse " << Console::WHITE << "gol help "
        << Console::FAINT_LIGHT_BLUE << "<command>" << Console::DEFAULT
        << " for detailed documentation.\n\n";
}