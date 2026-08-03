// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TilesetCatalog.h"
#include <clarisma/util/Pointers.h>
#include <clarisma/util/TritStreamReader.h>
#include <geodesk/query/TileIndexWalker.h>

using namespace clarisma;

void TilesetCatalog::open(const char* fileName)
{
    file_.open(fileName, File::OpenMode::READ);
    uint64_t size = file_.size();
    if (size < MIN_FILE_SIZE) fileCorrupted();
    start_ = reinterpret_cast<const uint8_t*>(
        file_.map(0, size));
    end_ = start_ + size;
    uint32_t tableSize = header()->tableSize;
    if (size < MIN_FILE_SIZE + tableSize * sizeof(uint32_t)) fileCorrupted();
}

void TilesetCatalog::close()
{
    if (start_) file_.unmap(start_, end_ - start_);
    start_ = end_ = nullptr;
    if (file_.isOpen()) file_.tryClose();
}

void TilesetCatalog::checkPointer(const uint8_t* ptr) const
{
    // TODO
}

void TilesetCatalog::fileCorrupted() const
{
    throw std::runtime_error("Tileset Catalog invalid");
}

bool TilesetCatalog::lookup(std::string_view base, std::string_view path,
        Tileset* tileset) const
{
    return false;   // TODO
}


size_t TilesetCatalog::hashWithoutHyphens(std::string_view id)
{
    size_t hash = 5381;  // Initial value for djb2 algorithm
    for (size_t i = 0; i < id.size(); ++i)
    {
        char ch = id[i];
        size_t newHash = ((hash << 5) + hash) +
            static_cast<unsigned char>(ch);
        hash = (ch == '-') ? hash : newHash;
    }
    return hash;
}

bool TilesetCatalog::equalWithoutHyphens(std::string_view id1, std::string_view id2)
{
    const char* p1 = id1.data();
    const char* end1 = p1 + id1.size();
    const char* p2 = id2.data();
    const char* end2 = p2 + id2.size();

    for (;;)
    {
        while (p1 < end1 && *p1 == '-') ++p1;
        while (p2 < end2 && *p2 == '-') ++p2;

        if (p1 == end1 || p2 == end2)
            return p1 == end1 && p2 == end2;

        if (*p1++ != *p2++) return false;
    }
}

const uint8_t* TilesetCatalog::lookupId(std::string_view id) const
{
    size_t hash = hashWithoutHyphens(id);
    size_t slot = hash % header()->tableSize;
    const uint8_t* p = start_ + header()->table[slot];
    uint8_t lenAndFlag;
    do
    {
        checkPointer(p + MIN_ENTRY_SIZE);
        lenAndFlag = *p++;
        uint8_t len = lenAndFlag >> 1;
        checkPointer(p + len);
        const char* s = reinterpret_cast<const char*>(p);

    }
    while (lenAndFlag & 1);
}


void TilesetCatalog::gatherTiles(const uint8_t *p, HashSet<Tile>* tiles) const
{
    const uint8_t* adjustedEnd = Pointers::masked(end_, ~7) +
        Pointers::extractBits(p, 7);
        // ensure that adjustedEnd - start_ is a multiple of 8
        // by adjusting end_ downward if needed (file ends with 8-byte trailer)
    TritStreamReader reader(p, adjustedEnd, 2);
    Tile levelBaseTiles[16];          // TODO: use CONSTANT
    uint8_t levelPos[16] = {};        // TODO: use CONSTANT
    int level = 1;
    int pos = 0;
    Tile baseTile = Tile::fromColumnRowZoom(0,0,1);
    // encoding skips the root tile; it is always implicitly PARTIAL
    for (;;)
    {
        int code = reader.next(level == maxLevel_);
            // At the final zoom level (typically 12),
            // we use bits instead of trits to encode tiles,
            // because we only need FULL or NONE; we cannot
            // have PARTIAL because all tiles at the max level
            // area leaves
        if (code == TileCoverage::FULL)
        {
            tiles->emplace(baseTile.relative(pos & 1, pos >> 1));
        }
        else if (code == TileCoverage::PARTIAL)
        {
            levelPos[level] = pos;
            levelBaseTiles[level] = baseTile;
            level++;
            assert(level <= maxLevel_);
            baseTile = Tile::fromColumnRowZoom(baseTile.column() * 2,
                baseTile.row() * 2, level);
            pos = 0;
            continue;
        }
        pos++;
        if (pos == 4)
        {
            level--;
            if (level == 0) break;
            baseTile = levelBaseTiles[level];
            pos = levelPos[level];
        }
    }
}
