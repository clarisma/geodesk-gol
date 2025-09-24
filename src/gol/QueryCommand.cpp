// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "QueryCommand.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <clarisma/cli/CliHelp.h>
#include <clarisma/io/FilePath.h>
#include <clarisma/validate/Validate.h>
#include <geodesk/format/KeySchema.h>

#include "gol/query/OutputFormat.h"
#include "gol/query/BriefQueryPrinter.h"
#include "gol/query/CountQueryPrinter.h"
#include "gol/query/CsvQueryPrinter.h"
#include "gol/query/GeoJsonQueryPrinter.h"
#include "gol/query/ListQueryPrinter.h"
#include "gol/query/WktQueryPrinter.h"
//#include "gol/query/TableQueryPrinter.h"
#include "gol/query/XmlQueryPrinter.h"

using namespace clarisma;
using namespace geodesk;

QueryCommand::Option QueryCommand::QUERY_OPTIONS[] =
{
    { "format",				OPTION_METHOD(&QueryCommand::setFormat) },
    { "f",	    			OPTION_METHOD(&QueryCommand::setFormat) },
    { "keys",				OPTION_METHOD(&QueryCommand::setKeys) },
    { "k",	    			OPTION_METHOD(&QueryCommand::setKeys) },
    { "precision",			OPTION_METHOD(&QueryCommand::setPrecision) },
    { "p",	    			OPTION_METHOD(&QueryCommand::setPrecision) }
};


QueryCommand::QueryCommand() :
    format_(OutputFormat::UNKNOWN),
    precision_(7)
{
    addOptions(QUERY_OPTIONS, sizeof(QUERY_OPTIONS) / sizeof(Option));
}


bool QueryCommand::setParam(int number, std::string_view value)
{
    if (number >= 2)
    {
        if (number > 2) query_ += " ";
        query_ += value;
        return true;
    }
    return GolCommand::setParam(number, value);
}

int QueryCommand::run(char* argv[])
{
    int res = GolCommand::run(argv);
    if (res != 0) return res;

    if (query_.empty())     [[unlikely]]
    {
        // TODO: No need to open GOL for interactive mode!
        interactive();
        return 0;
    }

    if(format_ == OutputFormat::UNKNOWN)
    {
        std::string path;
        if(!outputFileName_.empty())
        {
            path = outputFileName_;
        }
        else
        {
            path = File::path(Console::handle(Console::Stream::STDOUT));
        }
        const char* ext = FilePath::extension(path.c_str());
        if(*ext == '.') ext++;
        format_ = format(ext);
        format_ = format_ == OutputFormat::UNKNOWN ? OutputFormat::BRIEF : format_;
        // brief by default
    }
    else
    {
        // TODO :set output file extension based on format
        // TODO: fix, use string_view
        /*
        if(*FilePath::extension(outputFileName_) == 0)
        {
            outputFileName_
        }
        */
    }
    // TODO: What should the output be if OutputFormat::UNKNOWN?

    Console::get()->start("Querying...");

    const MatcherHolder* matcher = store_.getMatcher(query_.c_str());
    QuerySpec spec(&store_, bounds_, matcher->acceptedTypes(),
        matcher, filter_.get(), precision_, keys_);

    int64_t count = 0;
    switch (format_)
    {
    case OutputFormat::BRIEF:
        count = BriefQueryPrinter(&spec).run();
        break;
        /*
        case OutputFormat::CSV:
            new(&csv_)CsvPrinter(store, keys);
            break;
        */
    case OutputFormat::COUNT:
        count = CountQueryPrinter(&spec).run();
        break;
    case OutputFormat::CSV:
        count = CsvQueryPrinter(&spec).run();
        break;
    case OutputFormat::GEOJSON:
        count = GeoJsonQueryPrinter(&spec, false).run();
        break;
    case OutputFormat::GEOJSONL:
        count = GeoJsonQueryPrinter(&spec, true).run();
        break;
    case OutputFormat::LIST:
        count = ListQueryPrinter(&spec).run();
        break;
    /*
    case OutputFormat::TABLE:
        new(&table_)TablePrinter(store);
        break;
     */
    case OutputFormat::WKT:
        count = WktQueryPrinter(&spec).run();
        break;
    case OutputFormat::XML:
        count = XmlQueryPrinter(&spec).run();
        break;
    default:
        throw std::runtime_error("Format not yet implemented.");
    }

    // TODO: release matcher

    Console::end().success() << "Found "
        << Console::FAINT_LIGHT_BLUE
        << FormattedLong(count) << Console::DEFAULT
        << (count==1 ? " feature.\n" : " features.\n");

    return 0;
}

void QueryCommand::help()
{
    CliHelp help;
    help.command("gol query <gol-file> <query> [<options>]",
        "Performs a GOQL query.");
    help.beginSection("Output Options:");
    help.option("-o, --output <file>", "Write results to a file");
    help.option("-f, --format <format>", "Output format:");
    help.optionValue("count", "Only count retrieved features");
    help.optionValue("csv", "Comma-delimited values");
    help.optionValue("geojson", "GeoJSON");
    help.optionValue("list", "List of IDs");
    help.optionValue("wkt", "Well-Known Text");
    help.option("-k, --keys <list>", "Restrict tags to the given keys (csv and geojson only)");
    help.option("-p, --precision <n>", "Precision of coordinate values (Default: 7)");
    help.endSection();
    areaOptions(help);
    generalOptions(help);
}

OutputFormat QueryCommand::format(std::string_view s)
{
    std::unordered_map<std::string_view, OutputFormat> map = {
        {"brief", OutputFormat::BRIEF},
        {"count", OutputFormat::COUNT},
        {"csv", OutputFormat::CSV},
        {"json", OutputFormat::GEOJSON},
        {"geojson", OutputFormat::GEOJSON},
        {"jsonl", OutputFormat::GEOJSONL},
        {"geojsonl", OutputFormat::GEOJSONL},
        {"ndjson", OutputFormat::GEOJSONL},
        {"list", OutputFormat::LIST},
        {"list", OutputFormat::TABLE},
        {"wkt", OutputFormat::WKT},
        {"xml", OutputFormat::XML},
    };
    auto it = map.find(s);
    if (it == map.end()) return OutputFormat::UNKNOWN;
    return it->second;
}

int QueryCommand::setFormat(std::string_view s)
{
    if(s.empty()) return 1;
    format_ = format(s);
    if (format_ == OutputFormat::UNKNOWN)
    {
        throw std::runtime_error("Invalid format");
    }
    return 1;
}

int QueryCommand::setKeys(std::string_view s)
{
    keys_ = s;
    return 1;
}

int QueryCommand::setPrecision(std::string_view s)
{
    precision_ = Validate::intValue(s.data(), 0, 15);
    return 1;
}

// TODO: In interactive mode, no need to open the GOL

void QueryCommand::interactive()
{
    std::string_view golName = FilePath::withoutExtension(FilePath::name(golPath_));

    DynamicStackBuffer<1024> script_;

#ifdef _WIN32
    script_ << "python -i -c \""
        "try:\n"
        "    from geodesk2 import *\n"
        "except ImportError:\n"
        "    r = input('GeoDesk for Python is not installed. Install it now? [Y/n]').strip()\n"
        "    if r not in ('','Y','y'):\n"
        "        quit()\n"
        "    import subprocess, sys\n"
        "    try:\n"
        "        res = subprocess.check_call([sys.executable,'-m','pip','install','geodesk2'])\n"
        "    except subprocess.CalledProcessError:\n"
        "        quit()\n"
        "    from geodesk2 import *\n";
    if (true)   // TODO: only if golName is a valid Pytohn identifier
    {
        script_ << golName << " = ";
    }
    script_ << "features = Features(r'" << golPath_ << "')\n\"";
#else
    script_ << "python3 -i -c '"
        "try:\n"
        "    from geodesk2 import *\n"
        "except ImportError:\n"
        "    r = input(\"GeoDesk for Python is not installed. Install it now? [Y/n]\").strip()\n"
        "    if r not in (\"\",\"Y\",\"y\"):\n"
        "        quit()\n"
        "    import subprocess, sys\n"
        "    try:\n"
        "        res = subprocess.check_call([sys.executable,\"-m\",\"pip\",\"install\",\"geodesk2\"])\n"
        "    except subprocess.CalledProcessError:\n"
        "        quit()\n"
        "    from geodesk2 import *\n";
    if (true)   // TODO: only if golName is a valid Pytohn identifier
    {
        script_ << golName << " = ";
    }
    script_ << "features = Features(r\"" << golPath_ << "\")\n'";
#endif
    script_.writeByte(0);  // important: must write 0-terminator

    // LOGS << "Invoking: " << script_;

    ConsoleWriter out;
    out << "Querying " << Console::FAINT_LIGHT_BLUE << golName
        << Console::DEFAULT << " - To exit, type "
        << Console::GOLDEN_YELLOW << "quit()"
        << Console::DEFAULT << " or press "
#ifdef _WIN32
        << Console::GOLDEN_YELLOW << "Ctrl-Z"
        << Console::DEFAULT << ", followed by "
        << Console::GOLDEN_YELLOW << "Enter"
        << Console::DEFAULT;
#else
        << Console::GOLDEN_YELLOW << "Ctrl-D"
        << Console::DEFAULT;
#endif
    out.flush();

    Console::get()->restore();
    bool notFound = false;
#ifdef _WIN32
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    BOOL success = CreateProcessA(
        nullptr,                  // app name
        (LPSTR)script_.data(),    // mutable command line
        nullptr, nullptr, FALSE,
        0,                        // creation flags
        nullptr, nullptr,         // env, dir
        &si, &pi);

    if (!success) return; // TODO
    WaitForSingleObject(pi.hProcess, INFINITE);
DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    notFound = (exitCode == 9009);
#else
    int result = std::system(script_.data());
    if (result == -1)
    {
        notFound = true;
    }
    else
    {
        int exitCode = WEXITSTATUS(result);
        notFound = (exitCode == 127); // 127 = command not found
    }
#endif
    if (notFound)
    {
        ConsoleWriter out;
#ifdef _WIN32
        out.failed() << "Python not found. Download here: https://www.python.org/downloads/windows";
#else
        out.failed() << "Python not found. Please install Python 3 using your package manager.";
#endif
    }
}