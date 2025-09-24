// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "BasicCommand.h"
#include "build/GolBuilder.h"

class BuildCommand : BasicCommand
{
public:
	BuildCommand();

	int run(char* argv[]) override;

private:
	static Option BUILD_OPTIONS[];

	bool setParam(int number, std::string_view value) override;
	int setOption(std::string_view name, std::string_view value) override;
	void help();

	int setAreaRules(std::string_view s)
	{
		settings().setAreaRules(s.data());
		return 1;
	}

	int setIdIndexing(std::string_view s)
	{
		settings().setKeepIndexes(true);
		return 0;
	}

	int setIndexedKeys(std::string_view s)
	{
		settings().setIndexedKeys(s.data());
		return 1;
	}

	int setKeyIndexMinFeatures(std::string_view s)
	{
		settings().setKeyIndexMinFeatures(Validate::intValue(s.data()));
		return 1;
	}

	int setLevels(std::string_view s)
	{
		settings().setLevels(s.data());
		return 1;
	}

	int setMaxKeyIndexes(std::string_view s)
	{
		settings().setMaxKeyIndexes(Validate::intValue(s.data()));
		return 1;
	}

	int setMaxStrings(std::string_view s)
	{
		settings().setMaxStrings(Validate::longValue(s.data()));
		return 1;
	}

	int setMaxTiles(std::string_view s)
	{
		settings().setMaxTiles(Validate::longValue(s.data()));
		return 1;
	}

	int setMinTileDensity(std::string_view s)
	{
		settings().setMinTileDensity(Validate::longValue(s.data()));
		return 1;
	}

	int setMinStringUsage(std::string_view s)
	{
		settings().setMinStringUsage(Validate::longValue(s.data()));
		return 1;
	}

	int setRTreeBranchSize(std::string_view s)
	{
		settings().setRTreeBranchSize(Validate::intValue(s.data()));
		return 1;
	}

	int setWaynodeIds(std::string_view s)
	{
		settings().setIncludeWayNodeIds(true);
		return 0;
	}

	int setUpdatable(std::string_view s)
	{
		settings().setIncludeWayNodeIds(true);
		settings().setKeepIndexes(true);
		return 0;
	}

	BuildSettings& settings() { return builder_.settings(); }

	GolBuilder builder_;
	std::string golPath_;
	std::string sourcePath_;
};
