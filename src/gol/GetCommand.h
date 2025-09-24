// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "GolCommand.h"
#include <vector>

class GetCommand : public GolCommand
{
public:
	GetCommand();

	int run(char* argv[]);

private:
	bool setParam(int number, std::string_view value) override;
	int setOption(std::string_view name, std::string_view value) override;

	std::string_view golName_;
	std::vector<std::string_view> tilesetNames_;
	std::string golPath_;
};