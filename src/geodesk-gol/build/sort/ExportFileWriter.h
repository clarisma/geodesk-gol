// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "build/util/ForeignRelationLookup.h"
#include <clarisma/io/File.h>

class ExportFileWriter
{
public:
	ExportFileWriter(const std::filesystem::path& path, size_t tileCount) :
		offsets_(tileCount)
	{
		file_.open(path, File::OpenMode::CREATE |
			File::OpenMode::WRITE | File::OpenMode::TRUNCATE);
		file_.write(&tileCount, 8);
		fileSize_ = sizeof(uint64_t) * (tileCount + 1);
		file_.seek(fileSize_);
	}

	~ExportFileWriter()
	{
		assert(offsets_.data() == nullptr);	
	}

	void write(int pile, Block<ForeignRelationLookup::Entry>&& lookup)
	{
		offsets_[pile-1] = fileSize_;
		size_t size = lookup.size();
		static_assert(sizeof(size_t) == 8, "64-bit architecture required");
		file_.write(&size, 8);
		file_.write(lookup);
		fileSize_ += size * sizeof(ForeignRelationLookup::Entry) + 8;
		assert(file_.size() == fileSize_);
	}

	void close()
	{
		file_.seek(8);
		file_.write(offsets_);
		file_.close();
		offsets_ = Block<uint64_t>();
	}

private:
	File file_;
	uint64_t fileSize_;
	Block<uint64_t> offsets_;
};
