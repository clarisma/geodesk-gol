// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileSaver.h"
#include <clarisma/cli/Console.h>
#include <clarisma/util/Crc32C.h>
#include <clarisma/zip/Zip.h>
#include <geodesk/query/TileIndexWalker.h>
#include "tile/model/TileModel.h"
#include "tile/model/TileReader.h"
#include "tile/tes/TesArchive.h"

// TODO: Never save stale tiles to a TES Archive!

TileSaver::TileSaver(FeatureStore* store, int threadCount) :
	TaskEngine(threadCount),
	store_(store),
	workCompleted_(0),
	workPerTile_(0),
	totalBytesWritten_(0),
	entryCount_(0)
{
}

void TileSaver::save(const char* fileName, std::vector<std::pair<Tile,Tip>>& tiles)
{
	entryCount_ = static_cast<int>(tiles.size()) + 1;
	workPerTile_ = 100.0 / entryCount_;
	workCompleted_ = 0;

	Console::get()->start("Saving...");
	writer_.open(fileName, store_->guid(), store_->revision(),
		store_->revisionTimestamp(), entryCount_);
	start();

	for(const auto& tile : tiles)
	{
		postWork(TileSaverTask(tile.first, tile.second));
	}
	end();
	writer_.close();
}

void TileSaverWorker::processTask(TileSaverTask& task)
{
	FeatureStore* store = saver_->store_;
	DataPtr pTile = store->fetchTile(task.tip());
	TileModel tile;
	TileReader reader(tile);
	// store->prefetchBlob(pTile);
	reader.readTile(task.tile(), TilePtr(pTile));

	DynamicBuffer buf(1024 * 1024);
	TesWriter writer(tile, &buf);
	writer.write();
	saver_->postOutput(TileSaver::compressTile(task.tip(), buf.takeBytes()));
}


void TileSaver::writeMetadataSection(uint8_t*& p, TesMetadataType type, const void* src, size_t size)
{
	*p++ = static_cast<uint8_t>(type);
	writeVarint(p, size);
	memcpy(p, src, size);
	p += size;
}

ByteBlock TileSaver::gatherMetadata() const
{
	// TODO: Gather other metadata

	const FeatureStore::Header* header = store_->header();
	DataPtr mainMapping(reinterpret_cast<const uint8_t*>(header));
	DataPtr tileIndex = store_->tileIndex();
	size_t tileIndexSize = (store_->tipCount() + 1) * 4;
	DataPtr indexedKeys = mainMapping + header->indexSchemaPtr;
	size_t indexedKeysSize = (indexedKeys.getUnsignedInt() + 1) * 4;
	std::span<std::byte> stringTable = store_->stringTableData();
	std::span<std::byte> propertiesTable = store_->propertiesData();
	size_t maxMetadataSize = propertiesTable.size() +
		sizeof(FeatureStore::Settings) + tileIndexSize +
		indexedKeysSize + stringTable.size() + 16 * 5;
	std::unique_ptr<uint8_t[]> pMetadata(new uint8_t[maxMetadataSize]);

	ByteBlock blankTileIndex = createBlankTileIndex();
	assert(blankTileIndex.size() == tileIndexSize);
	uint8_t* p = pMetadata.get();

	writeMetadataSection(p, TesMetadataType::PROPERTIES, propertiesTable.data(), propertiesTable.size());
	writeMetadataSection(p, TesMetadataType::SETTINGS, &header->settings, sizeof(FeatureStore::Settings));
	writeMetadataSection(p, TesMetadataType::TILE_INDEX, blankTileIndex.data(), tileIndexSize);
	writeMetadataSection(p, TesMetadataType::INDEXED_KEYS, indexedKeys, indexedKeysSize);
	writeMetadataSection(p, TesMetadataType::STRING_TABLE, stringTable.data(), stringTable.size());

	size_t actualMetadataSize = p - pMetadata.get();
	assert(actualMetadataSize <= maxMetadataSize);
	return ByteBlock(std::move(pMetadata), actualMetadataSize);
}


ByteBlock TileSaver::createBlankTileIndex() const
{
	DataPtr tileIndex = store_->tileIndex();
	int tipCount = store_->tipCount();
	size_t tileIndexSize = (tipCount+1) * 4;
	std::unique_ptr<uint8_t[]> blankTileIndex(new uint8_t[tileIndexSize]);
	memcpy(blankTileIndex.get(), tileIndex, tileIndexSize);
	TileIndexWalker tiw(tileIndex, store_->zoomLevels(), Box::ofWorld(), nullptr);
	do
	{
		MutableDataPtr(blankTileIndex.get() + tiw.currentTip() *4).putInt(0);
	}
	while(tiw.next());
	return ByteBlock(std::move(blankTileIndex), tileIndexSize);
}

TileData TileSaver::compressTile(Tip tip, ByteBlock&& data)
{
	ByteBlock compressed = Zip::compressSealedChunk(data);
	LOGS << "Compressed " << data.size() << " bytes into " << compressed.size();
	uint32_t compressedSize = static_cast<uint32_t>(compressed.size());
		// Get size here, because compressed.take() sets compressed.size to 0
	return { tip, compressed.take(), compressedSize };
}

void TileSaver::preProcessOutput()
{
	ByteBlock data = gatherMetadata();
	writer_.writeMetadata(compressTile(Tip(), std::move(data)));
}

void TileSaver::processTask(TileData& task)
{
	writer_.writeTile(std::move(task));
	workCompleted_ += workPerTile_;
	Console::get()->setProgress(static_cast<int>(workCompleted_));
	totalBytesWritten_ += task.size();
}


