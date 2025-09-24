// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <unordered_map>
#include <clarisma/cli/CliCommand.h>
#include <clarisma/cli/Console.h>
#include <clarisma/cli/VerbosityLevel.h>

namespace clarisma {
class CliHelp;
}

#define OPTION_METHOD(m) static_cast<OptionMethodPtr>(m)

using clarisma::Console;

class BasicCommand : clarisma::CliCommand
{
public:
	BasicCommand();

	int threadCount() const { return threadCount_; }

	int run(char* argv[]);

protected:
	using OptionMethodPtr = int (BasicCommand::*)(std::string_view);
	struct Option
	{
		std::string_view name;
		OptionMethodPtr method;
	};

	static Option BASIC_OPTIONS[];

	void addOptions(const Option* options, size_t count);
	int setOption(std::string_view name, std::string_view value) override;
	int setThreads(std::string_view value);
	int setYesToAllPrompts(std::string_view)
	{
		yesToAllPrompts_ = true;
		return 0;
	}

	int setNoColor(std::string_view)	// NOLINT make static
	{
		clarisma::Console::get()->enableColor(false);
		return 0;
	}

	int setSilent(std::string_view)		// NOLINT: option setter cannot be static
	{
		setVerbosity(Console::Verbosity::SILENT);
		return 0;
	}

	int setQuiet(std::string_view)		// NOLINT: option setter cannot be static
	{
		setVerbosity(Console::Verbosity::QUIET);
		return 0;
	}

	int setVerbose(std::string_view)	// NOLINT: option setter cannot be static
	{
		setVerbosity(Console::Verbosity::VERBOSE);
		return 0;
	}

	int setDebug(std::string_view)		// NOLINT: option setter cannot be static
	{
		setVerbosity(Console::Verbosity::DEBUG);
		return 0;
	}

	static void setVerbosity(Console::Verbosity verbosity)
	{
		Console::setVerbosity(verbosity);
	}

	void generalOptions(clarisma::CliHelp& help);

	int threadCount_;
	bool yesToAllPrompts_;
	std::unordered_map<std::string_view,OptionMethodPtr> options_;
};
