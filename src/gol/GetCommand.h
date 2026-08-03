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

protected:
	bool setParam(int number, std::string_view value) override;

private:
	static Option OPTIONS[];

	int setListAction(std::string_view)
	{
		doList_ = true;
		return 0;
	}
	int setMapAction(std::string_view)
	{
		doList_ = true;
		return 0;
	}
	int setUpdateList(std::string_view)
	{
		updateList_ = true;
		return 0;
	}

	std::string_view url_;
	bool doList_ = false;
	bool doMap_ = false;
	bool updateList_ = false;
};