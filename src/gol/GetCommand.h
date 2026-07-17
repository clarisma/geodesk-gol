// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "LoadCommand.h"
#include <vector>

class GetCommand : public LoadCommand
{
public:
	GetCommand();

	int run(char* argv[]);

private:
	bool setParam(int number, std::string_view value) override;
	int setOption(std::string_view name, std::string_view value) override;

	std::string_view url_;
};