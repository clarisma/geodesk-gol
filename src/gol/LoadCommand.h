// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "GolCommand.h"
#include <vector>

class LoadCommand : public GolCommand
{
public:
	LoadCommand();

	int run(char* argv[]) override;

private:
	bool setParam(int number, std::string_view value) override;
	int setOption(std::string_view name, std::string_view value) override;
	void help() override;

	std::vector<std::string> tesFileNames_;
};