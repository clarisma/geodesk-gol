// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "GetIdCommand.h"
#include <clarisma/cli/CliHelp.h>
#include <clarisma/text/Csv.h>
#include <clarisma/text/TextMetrics.h>
#include <clarisma/util/StringBuilder.h>
#include <clarisma/validate/Validate.h>
#include <geodesk/feature/IdIndex.h>
#include <geodesk/feature/Tags.h>
#include <geodesk/format/FeatureRow.h>
#include <geodesk/format/GeoJsonFormatter.h>
#include <geodesk/format/WktFormatter.h>

using namespace clarisma;
using namespace geodesk;

GetIdCommand::Option GetIdCommand::OPTIONS[] =
{
    { "format",     OPTION_METHOD(&GetIdCommand::setFormat) },
    { "f",          OPTION_METHOD(&GetIdCommand::setFormat) },
    { "keys",       OPTION_METHOD(&GetIdCommand::setKeys) },
    { "k",          OPTION_METHOD(&GetIdCommand::setKeys) },
    { "precision",  OPTION_METHOD(&GetIdCommand::setPrecision) },
    { "p",          OPTION_METHOD(&GetIdCommand::setPrecision) },
};

GetIdCommand::GetIdCommand()
{
    addOptions(OPTIONS, sizeof(OPTIONS) / sizeof(Option));
}

bool GetIdCommand::setParam(int number, std::string_view value)
{
    if (number <= 1) return GolCommand::setParam(number, value);

    FeatureType type;
    uint64_t id;
    if (!parseTypedId(value, type, id))
    {
        throw std::runtime_error(
            std::string("Invalid ID format '") + std::string(value) +
            "'. Use n123, w456, or r789");
    }
    ids_.emplace_back(type, id);
    return true;
}

int GetIdCommand::setFormat(std::string_view s)
{
    if (s.empty()) return 1;
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
        {"wkt", OutputFormat::WKT},
    };
    auto it = map.find(s);
    if (it == map.end())
    {
        throw std::runtime_error("Invalid format. Use: brief, count, list, geojson, geojsonl, wkt, csv");
    }
    format_ = it->second;
    return 1;
}

int GetIdCommand::setKeys(std::string_view s)
{
    keys_ = s;
    return 1;
}

int GetIdCommand::setPrecision(std::string_view s)
{
    precision_ = Validate::intValue(s.data(), 0, 15);
    return 1;
}

bool GetIdCommand::parseTypedId(std::string_view arg,
    FeatureType& type, uint64_t& id)
{
    if (arg.empty()) return false;

    switch (arg[0])
    {
        case 'n': case 'N': type = FeatureType::NODE; break;
        case 'w': case 'W': type = FeatureType::WAY; break;
        case 'r': case 'R': type = FeatureType::RELATION; break;
        default: return false;
    }

    auto idStr = arg.substr(1);
    if (idStr.empty()) return false;

    id = 0;
    for (char c : idStr)
    {
        if (c < '0' || c > '9') return false;
        if (id > (UINT64_MAX - (c - '0')) / 10) return false;
        id = id * 10 + (c - '0');
    }
    return true;
}

int GetIdCommand::run(char* argv[])
{
    int res = GolCommand::run(argv);
    if (res != 0) return res;

    if (ids_.empty())
    {
        throw std::runtime_error(
            "No IDs specified. Usage: gol get-id <file> n123 w456 ...");
    }

    IdIndex* idx = store_.idIndex();
    if (!idx || !idx->isAvailable())
    {
        throw std::runtime_error(
            "ID lookups require index files. Rebuild with: gol build -i ...");
    }

    // Default keys for CSV if not specified
    if (format_ == OutputFormat::CSV && keys_.empty())
    {
        keys_ = "id,lon,lat,tags";
    }

    Console::get()->start("Looking up IDs...");

    // Collect found features
    std::vector<FeaturePtr> features;
    for (auto& [type, id] : ids_)
    {
        FeaturePtr ptr = idx->findById(id, type);
        if (!ptr.isNull())
        {
            features.push_back(ptr);
        }
    }

    // Output based on format
    switch (format_)
    {
    case OutputFormat::COUNT:
        // No output, just count
        break;
    case OutputFormat::LIST:
        printList(features);
        break;
    case OutputFormat::GEOJSON:
        printGeoJson(features, false);
        break;
    case OutputFormat::GEOJSONL:
        printGeoJson(features, true);
        break;
    case OutputFormat::WKT:
        printWkt(features);
        break;
    case OutputFormat::CSV:
        printCsv(features);
        break;
    case OutputFormat::BRIEF:
    default:
        printBrief(features);
        break;
    }

    int64_t count = static_cast<int64_t>(features.size());
    Console::end().success() << "Found "
        << Console::FAINT_LIGHT_BLUE << FormattedLong(count)
        << Console::DEFAULT
        << (count == 1 ? " feature.\n" : " features.\n");

    return 0;
}

void GetIdCommand::printBrief(const std::vector<FeaturePtr>& features)
{
    // Match BriefQueryPrinter colors exactly
    constexpr AnsiColor KEY_COLOR{"\033[38;5;137m"};
    constexpr AnsiColor GRAY{"\033[38;5;239m"};
    constexpr AnsiColor LIGHTGRAY{"\033[38;5;245m"};
    constexpr AnsiColor NODE_COLOR{"\033[38;5;147m"};
    constexpr AnsiColor WAY_COLOR{"\033[38;5;121m"};
    constexpr AnsiColor RELATION_COLOR{"\033[38;5;135m"};
    constexpr AnsiColor TYPE_COLORS[3] = { NODE_COLOR, WAY_COLOR, RELATION_COLOR };

    // Calculate max key width for alignment (like BriefQueryPrinter)
    int maxKeyWidth = 0;
    for (FeaturePtr feature : features)
    {
        Tags tags(&store_, feature);
        for (Tag tag : tags)
        {
            maxKeyWidth = std::max(maxKeyWidth,
                static_cast<int>(TextMetrics::countCharsUtf8(tag.key())));
        }
    }

    ConsoleWriter out;
    out.blank();
    for (FeaturePtr feature : features)
    {
        out << TYPE_COLORS[feature.typeCode()] << feature.typeName()
            << GRAY << '/' << LIGHTGRAY << feature.id() << "\n";

        Tags tags(&store_, feature);
        for (Tag tag : tags)
        {
            int w = static_cast<int>(TextMetrics::countCharsUtf8(tag.key()));
            out << "  " << KEY_COLOR << tag.key();
            out.writeRepeatedChar(' ', maxKeyWidth - w);
            out << GRAY << " = " << Console::DEFAULT;
            out << tag.value() << "\n";
        }
    }
}

void GetIdCommand::printList(const std::vector<FeaturePtr>& features)
{
    ConsoleWriter out;
    out.blank();
    for (FeaturePtr feature : features)
    {
        out << feature.typeName()[0] << feature.id() << "\n";
    }
}

void GetIdCommand::printGeoJson(const std::vector<FeaturePtr>& features, bool linewise)
{
    GeoJsonFormatter formatter;
    formatter.precision(precision_);

    ConsoleWriter out;
    out.blank();

    if (!linewise)
    {
        out << "{\"type\":\"FeatureCollection\",\"generator\":\"geodesk-gol\",\"features\":[";
    }

    bool first = true;
    DynamicBuffer buf(4096);
    for (FeaturePtr feature : features)
    {
        buf.clear();
        formatter.writeFeature(buf, &store_, feature);

        if (linewise)
        {
            out.write(buf.data(), buf.length());
            out.writeByte('\n');
        }
        else
        {
            if (!first) out.writeByte(',');
            first = false;
            out.write(buf.data(), buf.length());
        }
    }

    if (!linewise)
    {
        out << "]}";
    }
}

void GetIdCommand::printWkt(const std::vector<FeaturePtr>& features)
{
    WktFormatter formatter;
    formatter.precision(precision_);

    ConsoleWriter out;
    out.blank();

    if (features.empty())
    {
        out << "GEOMETRYCOLLECTION EMPTY\n";
        return;
    }

    if (features.size() == 1)
    {
        DynamicBuffer buf(4096);
        formatter.writeFeatureGeometry(buf, &store_, features[0]);
        out.write(buf.data(), buf.length());
        out.writeByte('\n');
        return;
    }

    // Multiple features -> GEOMETRYCOLLECTION
    out << "GEOMETRYCOLLECTION(";
    bool first = true;
    DynamicBuffer buf(4096);
    for (FeaturePtr feature : features)
    {
        buf.clear();
        formatter.writeFeatureGeometry(buf, &store_, feature);
        if (!first) out.writeByte(',');
        first = false;
        out.write(buf.data(), buf.length());
    }
    out << ")\n";
}

void GetIdCommand::printCsv(const std::vector<FeaturePtr>& features)
{
    KeySchema keys(&store_.strings(), keys_);

    ConsoleWriter out;
    out.blank();

    // Header row
    bool isFirst = true;
    for (auto header : keys.columns())
    {
        if (!isFirst) out.writeByte(',');
        isFirst = false;
        out << header;
    }
    out.writeByte('\n');

    // Data rows
    StringBuilder stringBuilder;
    DynamicBuffer csvBuf(1024);
    size_t colCount = keys.columnCount();
    for (FeaturePtr feature : features)
    {
        stringBuilder.clear();
        FeatureRow row(keys, &store_, feature, precision_, stringBuilder);
        for (size_t i = 0; i < colCount; i++)
        {
            if (i > 0) out.writeByte(',');
            csvBuf.clear();
            Csv::writeEscaped(csvBuf, row[i].toStringView());
            out.write(csvBuf.data(), csvBuf.length());
        }
        out.writeByte('\n');
    }
}

void GetIdCommand::help()
{
    CliHelp help;
    help.command("gol get-id <gol-file> <id>... [options]",
        "Retrieves features by their OSM ID.");
    help.beginSection("Arguments:");
    help.option("<id>", "One or more IDs in format n123, w456, or r789");
    help.endSection();
    help.beginSection("Output Options:");
    help.option("-o, --output <file>", "Write results to a file");
    help.option("-f, --format <format>", "Output format:");
    help.optionValue("brief", "Default, with tags (colored)");
    help.optionValue("count", "Only count features");
    help.optionValue("list", "List of type/id pairs");
    help.optionValue("geojson", "GeoJSON FeatureCollection");
    help.optionValue("geojsonl", "Newline-delimited GeoJSON");
    help.optionValue("wkt", "Well-Known Text");
    help.optionValue("csv", "Comma-separated values");
    help.option("-k, --keys <list>", "Columns for CSV (default: id,lon,lat,tags)");
    help.option("-p, --precision <n>", "Coordinate precision (default: 7)");
    help.endSection();
    help.beginSection("Examples:");
    help.option("gol get-id world.gol w327189648", "Get way 327189648");
    help.option("gol get-id world.gol n1 w2 r3", "Get multiple features");
    help.option("gol get-id world.gol w123 -f geojson", "Output as GeoJSON");
    help.endSection();
    generalOptions(help);
}
