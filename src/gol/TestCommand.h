// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "BasicCommand.h"

class TestCommand : public BasicCommand
{
public:
	int run(char* argv[]) override;

protected:
	bool setParam(int number, std::string_view value) override;

private:
	void testContents();

	const char* fileName_ = nullptr;
	const char* testName_ = nullptr;
};
