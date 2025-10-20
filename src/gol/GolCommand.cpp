// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "GolCommand.h"
#include <clarisma/cli/CliHelp.h>
#include <clarisma/io/FilePath.h>
#include <clarisma/util/StringBuilder.h>

#include "util/BoxParser.h"
#include "util/PolygonParser.h"

GolCommand::Option GolCommand::COMMON_GOL_OPTIONS[] =
{
	{ "area",	OPTION_METHOD(&GolCommand::setAreaOption) },
	{ "a",		OPTION_METHOD(&GolCommand::setAreaOption) },
	{ "box",	OPTION_METHOD(&GolCommand::setBox) },
	{ "b",		OPTION_METHOD(&GolCommand::setBox) },
	{ "circle",	OPTION_METHOD(&GolCommand::setCircle) },
	{ "c",		OPTION_METHOD(&GolCommand::setCircle) },
	{ "output",	OPTION_METHOD(&GolCommand::setOutput) },
	{ "o",		OPTION_METHOD(&GolCommand::setOutput) },
};

GolCommand::GolCommand()
{
	addOptions(COMMON_GOL_OPTIONS, sizeof(COMMON_GOL_OPTIONS) / sizeof(Option));
}

GolCommand::~GolCommand()
{
    try
    {
        store_.close();
        if (outputFile_.isOpen()) {
            outputFile_.close();
            File::rename(outputTmpFileName_.c_str(), outputFileName_.c_str());
        }
    }
    catch(...)
    {
        // do nothing
    }
}

bool GolCommand::setParam(int number, std::string_view value)
{
	if (number == 0) return true;   // command itself
	if (number == 1)
	{
		golPath_ = FilePath::withDefaultExtension(value, ".gol");
		return true;
	}
	return false;
}

void GolCommand::setAreaFromFile(const char* path)
{
	std::string pathWithExt = FilePath::withDefaultExtension(path, ".wkt");
	std::string content = File::readString(pathWithExt.c_str());
	setAreaFromCoords(content.c_str());
}

void GolCommand::setAreaFromCoords(const char* coords)
{
	PolygonParser parser(coords);
	filter_ = parser.parse();
	bounds_ = filter_->getBounds();
}

int GolCommand::setAreaOption(std::string_view value)
{
	if(!value.empty()) setArea(value.data());
	return 1;
}

void GolCommand::setArea(const char *value)
{
	// If the argument starts with @, it is a file name
	if(*value == '@')
	{
		setAreaFromFile(value+1);
		return;
	}

	// Use a heuristic to determine if the value contains literal
	// coordinates (separated by comma, space or tab)
	// If not, treat the value as a file name
	const char* s = value;
	for(;;)
	{
		char ch = *s++;
		if(ch == 0)
		{
			setAreaFromFile(value);
			return;
		}
		if(ch == ',' || ch == ' ' || ch == '\t') break;
	}
	setAreaFromCoords(value);
}

int GolCommand::setBox(std::string_view value)
{
	bounds_ = BoxParser(value.data()).parse();
	return 1;
}

int GolCommand::setCircle(std::string_view value)
{
	// TODO
	return 1;
}


int GolCommand::setOutput(std::string_view value)
{
	if(!value.empty())
	{
		outputFileName_ = value.data();
		outputTmpFileName_ = outputFileName_ + ".tmp";
	}
	return 1;
}


void GolCommand::areaOptions(CliHelp& help)
{
	help.beginSection("Area Options:");
	help.option("-a, --area <coords> | <file>","Restrict to polygon");
	help.option("-b, --bbox <W>,<S>,<E>,<N>","Restrict to bounding box");
	help.option("-c, --circle <m>,<lon>,<lat>","Restrict to <m> meters around a point");
	help.endSection();
}

int GolCommand::promptCreate(std::string_view filePath)
{
	if(yesToAllPrompts_) return 1;
	ConsoleWriter out;
	out.arrow() << Console::FAINT_LIGHT_BLUE << FilePath::name(filePath)
		<< Console::DEFAULT << " does not exist. Create it?";
	return out.prompt(true);
}

// TODO: Derived commands need to specify write mode
// TODO: info/query currently don't report error if GOL missing
int GolCommand::run(char* argv[])
{
	int res = BasicCommand::run(argv); 
	if (res != 0) return res;

	if(!outputTmpFileName_.empty())
	{
		// TODO: NEW vs TRUNCATE based on --overwrite
		outputFile_.open(outputTmpFileName_,
			File::OpenMode::WRITE | File::OpenMode::CREATE |
				File::OpenMode::TRUNCATE);
		Console::setOutputFile(outputFile_.handle());
	}

	if(golPath_.empty())
	{
		help();
		return 2;
	}

	if (openMode_ != DO_NOT_OPEN)	[[likely]]
	{
		store_.open(golPath_.c_str(), openMode_);
	}
	return 0;
}


void GolCommand::checkTilesetGuids(
	const std::string& path1, const clarisma::UUID& guid1,
	const std::string& path2, const clarisma::UUID& guid2)
{
	if(guid1 != guid2)
	{
		StringBuilder buf;
		buf << guid1;
		// TODO
		/*
		buf << "Tileset mismatch:\n"
		    << FilePath::name(path1) << ": " << guid1 << "\n"
			<< FilePath::name(path2) << ": " << guid2;
		*/
		throw std::runtime_error(buf.toString());
	}
}


