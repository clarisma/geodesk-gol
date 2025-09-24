// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileLoader.h"
#include <clarisma/cli/Console.h>
#include <clarisma/cli/ConsoleWriter.h>
#include <clarisma/zip/Zip.h>
#include <geodesk/query/TileIndexWalker.h>
#include "tile/compiler/IndexSettings.h"
#include "tile/model/Layout.h"
#include "tile/model/THeader.h"
#include "tile/model/TileModel.h"
#include "tile/model/TileReader.h"
#include "tile/tes/TesArchive.h"
#include "tile/tes/TesParcel.h"
#include "tile/tes/TesReader.h"

// TODO: Set waynode_ids flag in store

TileLoader::TileLoader(FeatureStore* store, int numberOfThreads) :
	TaskEngine(numberOfThreads),
	transaction_(*store),
	workCompleted_(0),
	workPerTile_(0),
	totalBytesWritten_(0),
	bytesSinceLastCommit_(0)
{
}

void TileLoader::reportSuccess(int tileCount)
{
	char buf[64];
	Format::unsafe(buf, "%d tiles loaded.\n", tileCount);
	Console::end().success().writeString(buf);
}

void TileLoader::initStore(const TesArchiveHeader& header,
	ByteBlock&& compressedMetadata, uint32_t sizeUncompressed, uint32_t checksum)
{
	transaction_.begin();
	ByteBlock metadata = Zip::inflate(compressedMetadata.data(), compressedMetadata.size(), sizeUncompressed);
	Zip::verifyChecksum(metadata, checksum);
	const uint8_t* p = metadata.data();
	const uint8_t* end = p + sizeUncompressed;

	FeatureStore::Metadata md(header.guid);
	md.revision = header.revision;
		// TODO: revision timestamp
	int sectionsPresent = 0;
	while(p < end)
	{
		int sectionNumber = *p++;
		TesMetadataType section = static_cast<TesMetadataType>(sectionNumber);
		sectionsPresent |= 1 << sectionNumber;
		uint32_t sectionSize = readVarint32(p);
		switch(section)
		{
		case TesMetadataType::PROPERTIES:
			md.properties = p;
			md.propertiesSize = sectionSize;
			break;
		case TesMetadataType::SETTINGS:
			md.settings = reinterpret_cast<const FeatureStore::Settings*>(p);
			if(sectionSize != sizeof(FeatureStore::Settings))
			{
				LOGS << "Size of Settings is " << sectionSize << " instead of " << sizeof(FeatureStore::Settings) << "\n";
			}
			assert(sectionSize == sizeof(FeatureStore::Settings));
			break;
		case TesMetadataType::TILE_INDEX:
			assert(sectionSize % 4 == 0);
			tileIndex_.reset(new uint32_t[sectionSize / 4]);
			memcpy(tileIndex_.get(), p, sectionSize);
			break;
		case TesMetadataType::INDEXED_KEYS:
			md.indexedKeys = reinterpret_cast<const uint32_t*>(p);
			break;
		case TesMetadataType::STRING_TABLE:
			md.stringTable = p;
			md.stringTableSize = sectionSize;
			break;
		default:
				// Ignore other metadata // TODO
			break;
		}
		p += sectionSize;
	}

	if(sectionsPresent != 0x3E)   // TODO: use proper mask
	{
		throw std::runtime_error("Invalid metadata (missing sections)");
	}

	transaction_.setup(md);
	transaction_.commit();
}

int TileLoader::prepareLoad(const char *tesFileName)
{
	Console::get()->start("Loading...");

	file_.open(tesFileName, File::OpenMode::READ);
	// uint64_t fileSize = in.size();
	TesArchiveHeader header;
	file_.read(&header, sizeof(header));
	entryCount_ = header.entryCount;
	catalog_.reset(new TesArchiveEntry[entryCount_]);
	tiles_.reset(new Tile[header.entryCount]);
	file_.read(&catalog_[0], sizeof(TesArchiveEntry) * header.entryCount);
	// TODO: check if all data read
	std::unordered_map<Tip, int> tipToEntry;
	tipToEntry.reserve(header.entryCount);

	headerAndCatalogSize_ = sizeof(TesArchiveHeader) + sizeof(TesArchiveEntry) * header.entryCount;
	const TesArchiveEntry* metadataEntry = nullptr;
	uint64_t metadataOfs = 0;
	uint64_t ofs = headerAndCatalogSize_;

	for(int i=0; i<header.entryCount; i++)
	{
		TesArchiveEntry& entry = catalog_[i];
		tipToEntry.insert({entry.tip, i});
		if(entry.tip.isNull())
		{
			metadataEntry = &entry;
			metadataOfs = ofs;
		}
		entry.tip = Tip();
		ofs += entry.size;
	}

	FeatureStore& store = transaction_.store();
	if (store.isCreated())
	{
		if(!metadataEntry)
		{
			throw TesException("Can't create GOL: TES does not contain metadata");
		}
		file_.seek(metadataOfs);
		initStore(header, file_.readBlock(metadataEntry->size),
			metadataEntry->sizeUncompressed, metadataEntry->checksum);
	}
	else
	{
		transaction_.begin();
	}

	DataPtr tileIndex(reinterpret_cast<uint8_t*>(tileIndex_.get()));
	int tileCount = 0;
	TileIndexWalker tiw(tileIndex, store.zoomLevels(), Box::ofWorld(), nullptr);
	do
	{
		if((tileIndex + tiw.currentTip() * 4).getInt() == 0)
		{
			Tip tip = tiw.currentTip();
			auto it = tipToEntry.find(tip);
			if (it != tipToEntry.end())
			{
				LOGS << "Marking tip " << tip << ": " << tiw.currentTile() << "\n";
				int n = it->second;
				assert(catalog_[n].tip.isNull());
				catalog_[n].tip = tip;
				tiles_[n] = tiw.currentTile();
				tileCount++;
			}
		}
	}
	while (tiw.next());

	// TODO: verify header
	workPerTile_ = 100.0 / tileCount;
	workCompleted_ = 0;

	return tileCount;
}


void TileLoader::load()
{
	assert(file_.isOpen());

	start();

	uint64_t ofs = headerAndCatalogSize_;
	for(int i=0; i<entryCount_; i++)
	{
		TesArchiveEntry& entry = catalog_[i];
		if(!entry.tip.isNull())
		{
			TesParcelPtr parcel = TesParcel::create(
				entry.size, entry.sizeUncompressed, entry.checksum);
			file_.seek(ofs);
			file_.read(parcel->data(), parcel->size());
			// TODO: verify read
			postWork({ entry.tip, tiles_[i], std::move(parcel) });
		}
		ofs += entry.size;
	}
	end();
	transaction_.commit();
	transaction_.end();

	Console::end().success() << "Done.\n";
}


void TileLoaderWorker::processTask(TileLoaderTask& task)
{
	FeatureStore& store = loader_->transaction_.store();
	// TilePtr pTile = TilePtr(BlobPtr(store->fetchTile(task.tip())));
		// TODO: clean this up; fetchTile() should return a TilePtr
	// uint32_t size = pTile.getInt() & 0x3fff'ffff;
	// uint8_t* pLoadedTile = new uint8_t[size];

	TileModel tile;
	// store->prefetchBlob(pTile);
	// TileReader reader(tile);
	// reader.readTile(task.tile(), pTile);

	TesParcelPtr parcel = task.takeFirstParcel();
	tile.init(task.tile(), parcel->sizeUncompressed() * 2);

	while (parcel)
	{
		// tile.initTables(parcel->size() * 2);

		ByteBlock block = parcel->uncompress();
		parcel = parcel->takeNext();
		TesReader tesReader(tile);
		tesReader.read(block.data(), block.size());
	}

	const FeatureStore::Settings& settings = store.header()->settings;
	IndexSettings indexSettings(store.keysToCategories(),
		settings.rtreeBranchSize, settings.maxKeyIndexes,
		settings.keyIndexMinFeatures);
	THeader indexer(indexSettings);
	indexer.addFeatures(tile);
	indexer.setExportTable(tile.exportTable());
	indexer.build(tile);

	Layout layout(tile);
	indexer.place(layout);
	layout.flush();
	layout.placeBodies();

#ifdef _DEBUG
	tile.check();
	// reader.counts_.check(layout.counts_);
	loader_->addCounts(layout.counts_);
#endif

	uint8_t* newTileData = tile.write(layout);

	loader_->postOutput(TileLoaderOutputTask(task.tip(), 
		ByteBlock(std::move(std::unique_ptr<uint8_t[]>(newTileData)),
			static_cast<size_t>(layout.size()))));
}


void TileLoader::processTask(TileLoaderOutputTask& task)
{
	uint32_t page = transaction_.addBlob({task.data(), task.size()});
	tileIndex_[task.tip()] = TileIndexEntry(page, TileIndexEntry::CURRENT);

	// TODO: respect concurrency mode
	// TODO: increment tile count in current snapshot

	workCompleted_ += workPerTile_;
	Console::get()->setProgress(static_cast<int>(workCompleted_));
	totalBytesWritten_ += task.size();
	bytesSinceLastCommit_ += task.size();
	/*
	if(bytesSinceLastCommit_ > 4ULL * 1024 * 1024 * 1024)
	{
		transaction_.commit();
		bytesSinceLastCommit_ = 0;
	}
	*/
}

#ifdef _DEBUG
void TileLoader::addCounts(const ElementCounts subTotal)
{
	std::lock_guard<std::mutex> lock(counterMutex_);
	totalCounts_ += subTotal;
}
#endif