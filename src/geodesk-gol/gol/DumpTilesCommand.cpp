// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "DumpTilesCommand.h"
#include <clarisma/io/File.h>
#include <clarisma/io/FilePath.h>
#include <clarisma/text/Format.h>
#include <clarisma/util/Buffer.h>
#include <geodesk/feature/FeatureStore.h>
#include "tile/util/TileDumper.h"

int DumpTilesCommand::run(char* argv[])
{
	int res = GolCommand::run(argv);
	if (res != 0) return res;

	std::string folder = std::string(FilePath::withoutExtension(golPath_)) + "-tiles";
	Engine engine(store(), threadCount(), folder);
	engine.run();
	return 0;
}


void DumpTilesCommand::Engine::prepareTile(Tip tip, Tile tile)
{
	int tipPrefix = tip >> 12;
	if (tipFoldersCreated_.find(tipPrefix) == tipFoldersCreated_.end())
	{
		char buf[16];
		Format::hexUpper(buf, tipPrefix, 3);
		std::filesystem::create_directories(folder_ / buf);
		tipFoldersCreated_.insert(tipPrefix);
	}
}


void DumpTilesCommand::Engine::processTile(Tip tip, Tile tile)
{
	// File file;
	char subFolderName[16];
	char fileName[16];
	Format::hexUpper(subFolderName, tip >> 12, 3);
	Format::hexUpper(fileName, tip, 3);
	strcpy(&fileName[3], ".txt");
	// file.open((folder_ / subFolderName / fileName).toString().c_str(),
	//	File::CREATE | File::REPLACE_EXISTING | File::WRITE);
	FILE* file = fopen((folder_ / subFolderName / fileName).string().c_str(), "wt");
	FileBuffer buf(file, 64 * 1024);
	TileDumper dumper(&buf, &store());
	DataPtr pTile = store().fetchTile(tip);
	dumper.dump(tile, TilePtr(pTile));
	// fclose(file);  // Done by buffer
	postOutput(tip, ByteBlock());
}
