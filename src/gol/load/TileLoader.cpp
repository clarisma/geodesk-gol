// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileLoader.h"
#include <clarisma/cli/Console.h>
#include <clarisma/cli/ConsoleWriter.h>
#include <clarisma/util/FileVersion.h>
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

void TileLoader::load(const char *golFileName, const char *gobFileName, bool wayNodeIds)
{
	wayNodeIds_ = wayNodeIds;
	file_.open(gobFileName, File::OpenMode::READ);
	Console::get()->start("Loading...");

	TesArchiveHeader header;
	file_.readAll(&header, sizeof(header));
	verifyHeader(header);
	size_t catalogSize = sizeof(TesArchiveHeader) +
		sizeof(TesArchiveEntry) * header.tileCount +
		sizeof(uint32_t);
	catalog_.reset(new std::byte[catalogSize]);
	memcpy(catalog_.get(), &header, sizeof(header));
	file_.readAll(catalog_.get() + sizeof(TesArchiveHeader),
		catalogSize - sizeof(TesArchiveHeader));
	size_t checksumOfs = catalogSize - sizeof(uint32_t);
	if (Crc32C::compute(catalog_.get(), checksumOfs) !=
		*reinterpret_cast<uint32_t*>(catalog_.get() + checksumOfs))
	{
		throw std::runtime_error("Invalid GOB catalog checksum");
	}
	if (wayNodeIds)  [[unlikely]]
	{
		if ((header.flags & TesArchiveHeader::Flags::WAYNODE_IDS) == 0)
		{
			throw std::runtime_error("Bundle does not contain waynode IDs");
		}
	}

	FeatureStore& store = transaction_.store();
	store.open(golFileName,
		FeatureStore::OpenMode::WRITE |
		FeatureStore::OpenMode::CREATE |
		FeatureStore::OpenMode::TRY_EXCLUSIVE);
		// TODO: modes

	transaction_.begin();

	uint64_t ofs = catalogSize;

	if (store.isCreated())
	{
		// TODO: metadata must be present
		/*
		if(!metadataEntry)
		{
			throw TesException("Can't create GOL: TES does not contain metadata");
		}
		*/
		initStore(header, file_.readBlock(header.metadataChunkSize));
	}
	else
	{
		if (transaction_.header().guid != header.guid)
		{
			throw std::runtime_error("Incompatible tileset");
		}
		if (wayNodeIds)  [[unlikely]]
		{
			if (!store.hasWaynodeIds())
			{
				throw std::runtime_error("Library does not store waynode IDs");
			}
		}
	}

	ofs += header.metadataChunkSize;

	int tileCount = determineTiles();
	if (tileCount == 0)
	{
		Console::end().success() << "All tiles already loaded.\n";
		return;
	}

	workPerTile_ = 100.0 / tileCount;
	workCompleted_ = 0;

	ConsoleWriter().blank() << "Loading "
		<< Console::FAINT_LIGHT_BLUE << FormattedLong(tileCount)
		<< Console::DEFAULT << (tileCount == 1 ? " tile into " : " tiles into ")
		<< Console::FAINT_LIGHT_BLUE << store.fileName()
		<< Console::DEFAULT << " from "
		<< Console::FAINT_LIGHT_BLUE << golFileName
		<< Console::DEFAULT << ":\n";

	start();

	auto p = reinterpret_cast<const TesArchiveEntry*>(
		catalog_.get() + sizeof(TesArchiveHeader));
	auto pEnd = p + header.tileCount;

	while (p < pEnd)
	{
		Tile tile = tiles_[p->tip];
		if (!tile.isNull())
		{
			// TODO: I/O ops can throw, need to catch and handle!
			file_.seek(ofs);
			postWork({ p->tip, tile, file_.readBlock(p->size) });
		}
		ofs += p->size;
		++p;
	}
	end();
	transaction_.commit();
	transaction_.end();

	Console::end().success() << "Done.\n";
}


void TileLoader::reportSuccess(int tileCount)
{
	char buf[64];
	Format::unsafe(buf, "%d tiles loaded.\n", tileCount);
	Console::end().success().writeString(buf);
}

void TileLoader::initStore(const TesArchiveHeader& header, ByteBlock&& compressedMetadata)
{
	ByteBlock metadata = Zip::uncompressSealedChunk(compressedMetadata);
	const uint8_t* p = metadata.data();
	const uint8_t* end = p + metadata.size();

	FeatureStore::Metadata md(header.guid);
	std::unique_ptr<uint32_t[]> tileIndex;
	md.flags = wayNodeIds_ ? FeatureStore::Header::Flags::WAYNODE_IDS : 0;
	md.revision = header.revision;
	md.revisionTimestamp = header.revisionTimestamp;
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
		{
			assert(sectionSize % 4 == 0);
			tileIndex.reset(new uint32_t[sectionSize / 4]);
			memcpy(tileIndex.get(), p, sectionSize);
			break;
		}
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

	transaction_.setup(md, std::move(tileIndex));
}


void TileLoader::verifyHeader(const TesArchiveHeader& header)
{
	if (header.magic != TesArchiveHeader::MAGIC)
	{
		throw std::runtime_error("Not a Geo-Object Bundle");
	}

	FileVersion version (header.formatVersionMajor, header.formatVersionMinor);
	version.checkExact("GOB", FileVersion(2,0));

	if (header.tileCount > 8000000)	// TODO: make constant
	{
		throw std::runtime_error("Invalid GOB header");
	}
}

int TileLoader::determineTiles()
{
	uint32_t tipCount = transaction_.header().tipCount;
	tiles_.reset(new Tile[tipCount+1]);
	for (int i = 0; i <= tipCount; ++i)
	{
		tiles_[i] = Tile();
	}

	DataPtr tileIndex(reinterpret_cast<const uint8_t*>(transaction_.tileIndex()));
	int tileCount = 0;
	TileIndexWalker tiw(tileIndex, transaction_.store().zoomLevels(),
		bounds_, filter_);
	do
	{
		Tip tip = tiw.currentTip();
		if((tileIndex + tip * 4).getInt() == 0)
		{
			tiles_[tip] = tiw.currentTile();
			tileCount++;
		}
	}
	while (tiw.next());

	// Now we have the number of tiles we want to load

	if (tileCount)  [[likely]]
	{
		// Now we'll count the number of tiles we can actually load

		tileCount = 0;
		auto p = reinterpret_cast<const TesArchiveEntry*>(
			catalog_.get() + sizeof(TesArchiveHeader));
		auto end = p + reinterpret_cast<const TesArchiveHeader*>(
			catalog_.get())->tileCount;
		while (p < end)
		{
			if (!tiles_[p->tip].isNull())
			{
				tileCount++;
			}
			++p;
		}
	}
	return tileCount;
}


void TileLoaderWorker::processTask(TileLoaderTask& task)
{
	FeatureStore& store = loader_->transaction_.store();
	// TilePtr pTile = TilePtr(BlobPtr(store->fetchTile(task.tip())));
		// TODO: clean this up; fetchTile() should return a TilePtr
	// uint32_t size = pTile.getInt() & 0x3fff'ffff;
	// uint8_t* pLoadedTile = new uint8_t[size];

	TileModel tile;
	tile.wayNodeIds(loader_->wayNodeIds_);
	// store->prefetchBlob(pTile);
	// TileReader reader(tile);
	// reader.readTile(task.tile(), pTile);

	ByteBlock block = Zip::uncompressSealedChunk(task.data(), task.size());
	tile.init(task.tile(), block.size() * 2);	// TODO: just tileSize?

	TesReader tesReader(tile);
	tesReader.read(block.data(), block.size());

	/*
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
	*/

	const FeatureStore::Settings& settings =
		loader_->transaction_.header().settings;
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

	loader_->postOutput(TileData(task.tip(),
		std::move(std::unique_ptr<uint8_t[]>(newTileData)),
		static_cast<size_t>(layout.size())));
}


void TileLoader::processTask(TileData& task)
{
	transaction_.putTile(task.tip(), {task.data(), task.size()});
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