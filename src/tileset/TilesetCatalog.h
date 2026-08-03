// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/io/File.h>
#include <clarisma/data/HashSet.h>
#include <clarisma/util/DateTime.h>
#include <geodesk/geom/Tile.h>

using namespace geodesk;

class Tileset
{
public:
	std::string_view title() const { return title_; }
	bool contains(Tile tile) const;

private:
	std::string title_;
	clarisma::HashSet<Tile> tiles_;
};

class TilesetCatalog
{
public:
	struct Header
	{
		static constexpr uint32_t MAGIC = 0xE0F6501D;	//	(1D 50 F6 E0) "IDs of geo"

		uint32_t magic = MAGIC;
		uint16_t formatVersionMajor = 1;
		uint16_t formatVersionMinor = 0;
		clarisma::DateTime timestamp;
		uint32_t reserved[3] = {};
		uint32_t tableSize = 0;
		uint32_t table[1];			// variable size
	};

	enum TileCoverage { NONE, FULL, PARTIAL };

	~TilesetCatalog() { close(); };

	void open(const char* fileName);
	void close();
	bool lookup(std::string_view base, std::string_view path,
		Tileset* tileset) const;
	void checkPointer(const uint8_t* ptr) const;
	void fileCorrupted() const;

	static size_t hashWithoutHyphens(std::string_view id);
	static bool equalWithoutHyphens(std::string_view id1, std::string_view id2);

private:
	const Header* header() const { return reinterpret_cast<const Header*>(start_); }
	const uint8_t* lookupId(std::string_view id) const;
	void gatherTiles(const uint8_t *p, clarisma::HashSet<Tile>* tiles) const;

	static constexpr size_t MIN_ENTRY_SIZE = 16;
	static constexpr size_t MIN_FILE_SIZE = sizeof(Header) + MIN_ENTRY_SIZE;

	clarisma::File file_;
	const uint8_t* start_ = nullptr;
	const uint8_t* end_ = nullptr;
	int maxLevel_ = 12;		// TODO: store in header
};



