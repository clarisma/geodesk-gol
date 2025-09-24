// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include "FastTileCalculator.h"

class NodeCountTable
{
public:
    static const int ZOOM_LEVEL = 12;
    static const size_t GRID_EXTENT = 1 << ZOOM_LEVEL;
    static constexpr size_t TABLE_SIZE = (GRID_EXTENT * GRID_EXTENT) + 1;
    // +1 to count any rejected nodes outside of range

    // empty table
    NodeCountTable() {}     

    // Move constructor
    NodeCountTable(NodeCountTable&& other) noexcept :
        counts_(std::move(other.counts_))
    {
    }

    void allocateEmpty()
    {
        counts_ = std::make_unique<uint32_t[]>(TABLE_SIZE);
        clear();
    }

    void clear()
    {
        std::fill_n(counts_.get(), TABLE_SIZE, 0);
    }

    // Move assignment operator
    NodeCountTable& operator=(NodeCountTable&& other) noexcept
    {
        std::swap(counts_, other.counts_);
        return *this;
    }

    // Prohibit copy construction/assignment
    NodeCountTable(const NodeCountTable&) = delete;
    NodeCountTable& operator=(const NodeCountTable&) = delete;

    uint32_t& operator[](size_t index)
    {
        assert(index < TABLE_SIZE);
        return counts_[index];
    }

    uint32_t& cell(int col, int row)
    {
        return counts_[row * GRID_EXTENT + col];
    }

    const uint32_t& cell(int col, int row) const
    {
        return counts_[row * GRID_EXTENT + col];
    }

    uint32_t& cell(Tile tile)
    {
        assert(tile.zoom() == ZOOM_LEVEL);
        return cell(tile.column(), tile.row());
    }

    const uint32_t& operator[](size_t index) const
    {
        return counts_[index];
    }

    NodeCountTable& operator+=(NodeCountTable&& other)
    {
        if (!counts_)
        {
            std::swap(counts_, other.counts_);
        }
        else
        {
            for (int i = 0; i < TABLE_SIZE; i++)
            {
                counts_[i] += other.counts_[i];
            }
        }
        return *this;
    }

    void load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;

    const uint32_t* data() const { return counts_.get(); }

private:
    struct SavedCount
    {
        Tile tile;
        uint32_t count;
    };

    std::unique_ptr<uint32_t[]> counts_;
};

