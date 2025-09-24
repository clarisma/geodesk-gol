// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/io/MappedFile.h>
#include <clarisma/util/DateTime.h>
#include <clarisma/util/UUID.h>
#include <geodesk/feature/Tip.h>

using namespace geodesk;

enum class TesMetadataType
{
	PROPERTIES = 1,
	SETTINGS = 2,
	TILE_INDEX = 3,
	STRING_TABLE = 4,
	INDEXED_KEYS = 5
};

struct TesArchiveHeader
{
	uint32_t magic = 0xE0F6B060;	//	(60 B0 F6 E0) "gob of geo"
	uint16_t formatVersionMajor = 1;
	uint16_t formatVersionMinor = 9000;
	clarisma::UUID guid;
	uint32_t flags = 0;
	uint32_t entryCount =0 ;
	uint32_t baseRevision = 0;
	uint32_t revision = 0;
	clarisma::DateTime revisionTimestamp;
};

struct TesArchiveEntry
{
	TesArchiveEntry() : tip(0), size(0), sizeUncompressed(0), checksum(0) {}
	TesArchiveEntry(Tip tip, uint32_t size, uint32_t sizeUncompressed, uint32_t checksum)
		: tip(tip), size(size), sizeUncompressed(sizeUncompressed), checksum(checksum) {}

	Tip tip;
	uint32_t size;
	uint32_t sizeUncompressed;
	uint32_t checksum;
};


class TesArchive : protected clarisma::MappedFile
{
public:
	TesArchive() : data_(nullptr), fileSize_(0) {}
	void open(const char* fileName);
	void close();

	const TesArchiveHeader& header() const
	{
		return *reinterpret_cast<const TesArchiveHeader*>(data_);
	}

	const TesArchiveEntry& operator[](int n) const
	{
		assert(data_);
		assert(n >= 0 && n < header().entryCount);
		return *reinterpret_cast<const TesArchiveEntry*>(
			data_ + sizeof(TesArchiveHeader) + sizeof(TesArchiveEntry) * n);
	}

	const uint8_t* dataAtOffset(uint64_t ofs) const
	{
		assert(ofs >= sizeof(TesArchiveHeader) + sizeof(TesArchiveEntry) * header().entryCount);
		return data_ + ofs;
	}

	std::unique_ptr<uint64_t[]> computeOffsets() const;

private:
	uint8_t* data_;
	size_t fileSize_;
};



