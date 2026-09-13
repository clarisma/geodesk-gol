// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "BasicCommand.h"

#include <clarisma/cli/CliHelp.h>
#include <clarisma/validate/Validate.h>

using namespace clarisma;



BasicCommand::Option BasicCommand::BASIC_OPTIONS[] =
{
	{ "threads", &BasicCommand::setThreads },
	{ "Y", &BasicCommand::setYesToAllPrompts },
	{ "yes", &BasicCommand::setYesToAllPrompts },
	{ "no-color", &BasicCommand::setNoColor },
	{ "s", &BasicCommand::setSilent },
	{ "silent", &BasicCommand::setSilent },
	{ "q", &BasicCommand::setQuiet },
	{ "quiet", &BasicCommand::setQuiet },
	{ "v", &BasicCommand::setVerbose },
	{ "verbose", &BasicCommand::setVerbose },
	{ "d", &BasicCommand::setDebug },
	{ "debug", &BasicCommand::setDebug }
};

BasicCommand::BasicCommand() :
	threadCount_(0),
	yesToAllPrompts_(false)
{
	addOptions(BASIC_OPTIONS, sizeof(BASIC_OPTIONS) / sizeof(Option));
}

void BasicCommand::addOptions(const Option* options, size_t count)
{
	for (int i = 0; i < count; i++)
	{
		options_[options[i].name] = options[i].method;
	}
}

int BasicCommand::setThreads(std::string_view value)
{
	if(!value.empty())
	{
		threadCount_ = Validate::intValue(value.data(), 0,
			std::thread::hardware_concurrency() * 4);
	}
	return 1;
}

/*
int BasicCommand::setOption(std::string_view name, std::string_view value)
{
	if (name == "threads")
	{
		if(value.empty()) return 1;
		threadCount_ = Validate::intValue(value.data(), 0,
			std::thread::hardware_concurrency() * 4);
		return 1;
	}
	if (name == "Y" || name == "yes")
	{
		yesToAllPrompts_ = true;
		return 0;
	}

	if (name == "no-color")
	{
		Console::get()->enableColor(false);
		return 0;
	}
	return -1;
}
*/

int BasicCommand::setOption(std::string_view name, std::string_view value)
{
	auto it = options_.find(name);
	if (it == options_.end()) return -1;
	return (this->*(it->second))(value);
}

int BasicCommand::run(char* argv[])
{
	int res = CliCommand::run(argv);
	if (res != 0) return res;

	if (threadCount_ == 0) threadCount_ = std::thread::hardware_concurrency();
	return 0;
}


void BasicCommand::generalOptions(CliHelp& help)
{
	help.beginSection("General Options:");
	help.option("-s, --silent","No output");
	help.option("-q, --quiet","Minimal output");
	help.option("-v, --verbose","Detailed output");
	help.option("--color | --no-color","Enable/disable colored output");
	help.option("-Y, --yes","Dismiss all prompts with \"yes\"");
	help.option("--threads <n>","Number of worker threads");
	help.endSection();
}

