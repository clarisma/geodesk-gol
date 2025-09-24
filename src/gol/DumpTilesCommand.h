// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "GolCommand.h"
#include <filesystem>
#include <unordered_set>
#include "tile/util/TileTaskEngine.h"

class DumpTilesCommand : public GolCommand
{
public:
	int run(char* argv[]) override;

private:
	class Engine : public TileTaskEngine
	{
	public:
		Engine(FeatureStore& store, int threadCount, std::string_view folder) :
			TileTaskEngine(store, threadCount),
			folder_(folder)
		{
		}

		void prepareTile(Tip tip, Tile tile) override;
		void processTile(Tip tip, Tile tile) override;

	private:
		std::filesystem::path folder_;
		std::unordered_set<int> tipFoldersCreated_;		// main thread only
	};
};
