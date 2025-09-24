// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "NodeCountTable.h"
#include <clarisma/io/File.h>
#include <clarisma/io/FileBuffer3.h>

using namespace clarisma;

void NodeCountTable::load(const std::filesystem::path& path)
{
	if (!counts_.get()) allocateEmpty();
	File file;
	file.open(path, File::OpenMode::READ);
	uint64_t size = file.size();
	assert((size % sizeof(SavedCount)) == 0);
	std::unique_ptr<SavedCount[]> buf = 
		std::make_unique<SavedCount[]>(size / sizeof(SavedCount));
	uint64_t bytesRead = file.read(buf.get(), size);
	assert(bytesRead == size);
	const SavedCount* p = buf.get();
	const SavedCount* end = p + size / sizeof(SavedCount);
	while (p < end)
	{
		cell(p->tile) = p->count;
		p++;
	}
}

void NodeCountTable::save(const std::filesystem::path& path) const
{
	FileBuffer3 out;
	out.open(path);
	for (int row = 0; row < GRID_EXTENT; row++)
	{
		for (int col = 0; col < GRID_EXTENT; col++)
		{
			uint32_t count = cell(col, row);
			if (count)
			{
				SavedCount saved = { Tile::fromColumnRowZoom(col, row, ZOOM_LEVEL), count };
				out.write(&saved, sizeof(saved));
			}
		}
	}
	out.close();
}

