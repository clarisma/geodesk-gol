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

protected:
	static Option OPTIONS[];

	bool setParam(int number, std::string_view value) override;
	int setWaynodeIds(std::string_view s)
	{
		waynodeIds_ = true;
		return 0;
	}
	void help() override;

	std::string gobFileName_;
	bool waynodeIds_ = false;
	bool isRemoteGob_ = false;
};