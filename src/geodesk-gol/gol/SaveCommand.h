// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "GolCommand.h"
#include <vector>

class SaveCommand : public GolCommand
{
public:
	SaveCommand();

	int run(char* argv[]) override;

private:
	static Option OPTIONS[];

	bool setParam(int number, std::string_view value) override;
	// int setOption(std::string_view name, std::string_view value) override;
	int setWaynodeIds(std::string_view s)
	{
		waynodeIds_ = true;
		return 0;
	}
	void help() override;

	std::string gobPath_;
	bool waynodeIds_ = false;
};