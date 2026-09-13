// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "GolCommand.h"
#include <filesystem>
#include <unordered_set>
#include "tile/util/TileTaskEngine.h"

class CheckCommand : public GolCommand
{
public:
	int run(char* argv[]) override;
};
