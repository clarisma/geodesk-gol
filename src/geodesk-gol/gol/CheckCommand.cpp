// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "CheckCommand.h"

#include <clarisma/io/FilePath.h>

#include "check/GolChecker.h"

int CheckCommand::run(char* argv[])
{
	int res = GolCommand::run(argv);
	if (res != 0) return res;

	std::string shortName(FilePath::name(store().fileName()));
	ConsoleWriter out;
	out << "Checking " << Console::FAINT_LIGHT_BLUE
		<< shortName << Console::DEFAULT << ":";
	out.flush();

	GolChecker checker(store(), threadCount());
	// Console::get()->start("Checking tiles...");

	checker.run();
	Console::end().success() << "No errors found\n";
	return 0;
}
