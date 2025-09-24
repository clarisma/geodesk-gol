// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "build/util/ForeignRelationLookup.h"
#include <clarisma/data/Span.h>
#include <clarisma/io/MappedFile.h>

class ExportFile
{
public:
	ExportFile(const std::filesystem::path& path)
	{
		file_.open(path, File::OpenMode::READ);
		mapped_ = reinterpret_cast<uint8_t*>(file_.map(0, file_.size(), MappedFile::READ));
	}

	~ExportFile()
	{
		file_.unmap(mapped_, file_.size());
		file_.close();
	}

	Tex texOfRelation(int pile, uint64_t id)
	{
		size_t ofs = reinterpret_cast<const size_t*>(mapped_)[pile];
		auto pTable = reinterpret_cast<const ForeignRelationTable*>(mapped_ + ofs);
		auto entry = ForeignRelationLookup::lookup(pTable->asSpan(), id);
		if (entry == nullptr)
		{
			// TODO: SAFEMODE: Relation must be in table
			LOGS << "Relation " << id << " not found in Exports ("
				<< pTable->asSpan().size() << " rels searched in pile #"
				<< pile;
			assert(false);
		}
		return entry->tex;
	}

private:
	MappedFile file_;
	uint8_t* mapped_;
};
