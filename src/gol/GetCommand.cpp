// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "GetCommand.h"
#include "GolCommand.h"
#include <clarisma/cli/CliApplication.h>
#include "gol/load/TileLoader.h"
#include <geodesk/feature/FeatureStore.h>


GetCommand::GetCommand()
{

}

bool GetCommand::setParam(int number, std::string_view value)
{
	if(GolCommand::setParam(number, value)) return true;
	tesFileNames_.emplace_back(value);
	return true;
}

int GetCommand::setOption(std::string_view name, std::string_view value)
{
	// TODO
	return GolCommand::setOption(name, value);
}

int GetCommand::run(char* argv[])
{
	int res = GolCommand::run(argv);
	if (res != 0) return res;

	// TODO !!!!
	url_ = tesFileNames_[0];

	TileLoader downloader(&store_, threadCount());
	downloader.download(golPath_.c_str(), waynodeIds_,
		url_.data(), bounds_, filter_.get());
		// url_ is guaranteed to be null-terminated
	/*
	FeatureStore store;
	store.open(GolCommand::golPath(golName_).c_str());
	*/

	/*
	ConsoleWriter& out = CliApplication::get()->out();
	out.writeConstString("Checking ");
	out.color(111);
	out.writeConstString("https://data.geodesk.com");
	out.normal();
	out.writeConstString("\nFetching ");
	out.color(111);
	out.writeConstString("Sonoma County (California, USA)");
	out.normal();
	out.writeConstString("\n    from ");
	out.color(111);
	out.writeConstString("GeoDesk OSM Worldwide");
	out.normal();
	out.writeConstString("\nRevision ");
	out.color(111);
	out.writeConstString("129578");
	out.normal();
	out.writeConstString(" (2 minutes ago) - ");
	out.color(34);
	out.writeConstString("latest");
	out.normal();
	out.writeConstString("\n");
	out.flush();
	*/

	/*
	TileLoader loader(&store_, threadCount());
	loader.load();
	*/
	return 0;
}
