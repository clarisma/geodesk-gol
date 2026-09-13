// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/io/MappedFile.h>

class MappedIndex
{
public:
	~MappedIndex() { release(); }

	void create(const char* fileName, int64_t maxId, int valueWidth);
	void clear();
	void sync();
	void release();
	void close()
	{
		release();
		file_.close();
	}
	uint64_t* data() const { return index_; }
	int64_t maxId() const { return maxId_; }
	int valueWidth() const { return valueWidth_; }

	static const int64_t SEGMENT_LENGTH_BYTES = 1024 * 1024 * 1024;		// 1 GB
		// TODO: put this in a common place

private:
	uint64_t calculateMappingSize();

	uint64_t* index_ = nullptr;
	clarisma::MappedFile file_;
	int64_t maxId_ = 0;
	int valueWidth_ = 1;
        // cannot be 0, because otherwise calculateMappingSize()
        // will fail due to divide by zero
        // TODO: store mappingSize?
};
