// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileCompiler.h"
#include <clarisma/zip/Zip.h>
#include <geodesk/feature/FeatureStore.h>
#include <tile/model/THeader.h>
#include "IndexSettings.h"
#include "tile/model/Layout.h"
#include "tile/model/TileReader.h"
#include "tile/tes/TesArchive.h"
#include "tile/tes/TesReader.h"


void TileCompiler::modifyTile(Tip tip, Tile tile)
{
    DataPtr pTile = store_->fetchTile(tip);
    TileReader reader(tile_);
    reader.readTile(tile, TilePtr(pTile));
}

void TileCompiler::addChanges(std::span<const uint8_t> tesData)
{
    TesReader tesReader(tile_, store_->hasWaynodeIds());
    tesReader.read(tesData.data(), tesData.size());
}

void TileCompiler::addChanges(const TesArchiveEntry& entry, const uint8_t* data)
{
    ByteBlock block = Zip::uncompressSealedChunk(data, entry.size);
    addChanges(block);
}


ByteBlock TileCompiler::compile()
{
    const FeatureStore::Settings& settings = store_->header()->settings;
    IndexSettings indexSettings(store_->keysToCategories(),
        settings.rtreeBranchSize, settings.maxKeyIndexes,
        settings.keyIndexMinFeatures);
    THeader indexer(indexSettings);
    indexer.addFeatures(tile_);
    indexer.setExportTable(tile_.exportTable());
    indexer.build(tile_);

    Layout layout(tile_);
    indexer.place(layout);
    layout.flush();
    layout.placeBodies();

    uint8_t* newTileData = tile_.write(layout);
    return { newTileData, static_cast<size_t>(layout.size()) };
}