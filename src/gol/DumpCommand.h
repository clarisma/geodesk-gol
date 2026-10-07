// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "GolCommand.h"
#include <filesystem>
#include <unordered_set>
#include "tile/util/TileTaskEngine.h"

class DumpCommand : public GolCommand
{
public:
	int run(char* argv[]) override;
	bool setParam(int number, std::string_view value) override;

private:
	enum
	{
		STRINGS,
		FREE,
	};

	void dumpFreeRanges();

	int what_ = STRINGS;
};
