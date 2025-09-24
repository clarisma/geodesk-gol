// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "Analyzer.h"
#include "build/GolBuilder.h"
#include "build/util/StringCatalog.h"
#include <string>

#include "clarisma/io/FileBuffer3.h"

// TODO: Need to flush remaining strings at end
// TODO: race condition

// TODO: Must ensure that all strings have valid UTF-8 encoding,
//  otherwise creating Python strings can fail
//  The Analyzer should not allow build to proceed if there is
//  any bad UTF-8 data in the .osm.pbf

Analyzer::Analyzer(GolBuilder* builder) :
	OsmPbfReader(builder->threadCount()),
	builder_(builder),
	strings_(outputTableSize(), outputArenaSize()),
	minStringCount_(2)
{
}

AnalyzerWorker::AnalyzerWorker(Analyzer* analyzer) :
	OsmPbfContext<AnalyzerWorker, Analyzer>(analyzer),
	strings_(analyzer->workerTableSize(), analyzer->workerArenaSize())
{
	nodeCounts_.allocateEmpty();
}


void AnalyzerWorker::flush()
{
	LOG("== Flushing context %p with %d strings", this, strings_.counterCount());
	std::unique_ptr<uint8_t[]> strings = strings_.takeStrings();

	// Now that we've reset the String Statistics, the lookup table entries
	// are no longer valid -- we need to reset each counter offset to 0,
	// so the next lookup creates a new counter

	for (auto& entry : stringCodeLookup_) 
	{
		entry.counterOfs = 0;
	}
	 
	reader()->postOutput(AnalyzerOutputTask(strings.release(), blockBytesProcessed()));
	resetBlockBytesProcessed();
}

const uint8_t* AnalyzerWorker::node(int64_t id, int32_t lon100nd, int32_t lat100nd, ByteSpan tags)
{
	// TODO: Could use a cheaper projection function since coordinates just need
	// to be approximate

	/*
	int x = Mercator::xFromLon100nd(lon100nd);
	int y = Mercator::yFromLat100nd(lat100nd);
	int col = Tile::columnFromXZ(x, 12);
	int row = Tile::rowFromYZ(y, 12);
	nodeCounts_[row * 4096 + col]++;
	*/
	uint32_t cell = reader()->tileCalculator()->calculateCell(lon100nd, lat100nd);
	nodeCounts_[cell]++;

	const uint8_t* p = tags.data();
	while (p < tags.end())
	{
		uint32_t key = readVarint32(p);
		if (key == 0) break;
		uint32_t value = readVarint32(p);
		countString(key, 1, 0);
		countString(value, 0, 1);
		stats_.tagCount++;
	}
	stats_.nodeCount++;
	stats_.maxNodeId = id;	// assumes nodes are ordered by ID
		// TODO: This feels hacky; start/end should be immutable, and node()
		// should not have the responsibility to advance start pointer.
		// However, this is the fastest approach
	return p;
}


void AnalyzerWorker::way(int64_t id, ByteSpan keys, ByteSpan values, ByteSpan nodes)
{
	countStrings(keys, 1, 0);
	stats_.tagCount += countStrings(values, 0, 1);
	stats_.wayCount++;
	stats_.maxWayId = id;  // assumes ways are ordered by ID
}

void AnalyzerWorker::relation(int64_t id, ByteSpan keys, ByteSpan values,
	ByteSpan roles, ByteSpan memberIds, ByteSpan memberTypes)
{
	countStrings(keys, 1, 0);
	stats_.tagCount += countStrings(values, 0, 1);
	stats_.memberCount += countStrings(roles, 0, 1);
	stats_.relationCount++;
	stats_.maxRelationId = id;  // assumes relations are ordered by ID
}

void AnalyzerWorker::countString(uint32_t index, int keys, int values)
{
	assert(index < stringCodeLookup_.size());  // TODO: exception?
	StringLookupEntry& entry = stringCodeLookup_[index]; 
	if (entry.counterOfs == 0)
	{
		const ShortVarString* str = reinterpret_cast<const ShortVarString *>(
			stringTable_ + entry.stringOfs);
		StringStatistics::CounterOfs ofs = strings_.getCounter(str);
		if (ofs == 0)
		{
			flush();
			ofs = strings_.getCounter(str);
			// Second attempt must succeed
			assert(ofs);
		}
		entry.counterOfs = ofs;
	}
	strings_.counterAt(entry.counterOfs)->add(keys, values);
}

int AnalyzerWorker::countStrings(ByteSpan strings, int keys, int values)
{
	int numberOfStrings = 0;
	const uint8_t* p = strings.data();
	while (p < strings.end())
	{
		uint32_t strIndex = readVarint32(p);
		countString(strIndex, keys, values);
		numberOfStrings++;
	}
	return numberOfStrings;
}


void AnalyzerWorker::stringTable(ByteSpan strings)  // CRTP override
{
	stringTable_ = strings.data();
	const uint8_t* p = stringTable_;
	while (p < strings.end())
	{
		uint32_t marker = readVarint32(p);
		if (marker != OsmPbf::STRINGTABLE_ENTRY)
		{
			throw OsmPbfException("Bad string table. Unexpected field: %d", marker);
		}
		uint32_t ofs = p - stringTable_;
		p += readVarint32(p);
		stringCodeLookup_.push_back({ ofs, 0 });
	}
	assert(p == strings.end());
}


void AnalyzerWorker::endBlock()	// CRTP override
{
	stringCodeLookup_.clear();
}

void AnalyzerWorker::afterTasks()
{
	LOG("Context %p: flushing remaining strings...", this);
	flush();
}

void AnalyzerWorker::harvestResults()
{
	Analyzer* analyzer = reader();
	analyzer->totalNodeCounts() += std::move(nodeCounts_);
	analyzer->osmStats() += stats_;
}

void Analyzer::processTask(AnalyzerOutputTask& task)
{
	const uint8_t* arena = task.strings();
	uint32_t size = *reinterpret_cast<const uint32_t*>(arena);
	const uint8_t* arenaEnd = arena + size;
	const uint8_t* p = arena + sizeof(uint32_t);
	while (p < arenaEnd)
	{
		const StringStatistics::Counter* counter =
			reinterpret_cast<const StringStatistics::Counter*>(p);
		uint32_t stringSize = counter->string().totalSize();
		for(;;)
		{
			if (counter->totalCount() < minStringCount_) break;
			StringStatistics::CounterOfs ofs;
			ofs = strings_.getCounter(&counter->string(), counter->hash());
			if (ofs)
			{
				strings_.counterAt(ofs)->add(counter);
				break;
			}
			LOG("==== Global string arena full, culling strings < %d...", minStringCount_);
			strings_.removeStrings(minStringCount_);
			minStringCount_ <<= 1;
		}
		p += StringStatistics::Counter::grossSize(stringSize);
	}
	builder_->progress(task.blockBytesProcessed() * workPerByte_);
}


void Analyzer::addRequiredStrings()
{
	for (int i = 0; i < StringCatalog::CORE_STRING_COUNT; i++)
	{
		strings_.addRequiredCounter(std::string_view(StringCatalog::CORE_STRINGS[i]));
	}
	for (auto indexedKey : builder_->settings().indexedKeys())
	{
		strings_.addRequiredCounter(indexedKey.key);
	}
}

void Analyzer::startFile(uint64_t size)		// CRTP override
{
	workPerByte_ = builder_->phaseWork(GolBuilder::Phase::ANALYZE) /
		static_cast<double>(size);
	Console::get()->setTask("Analyzing...");
}


void Analyzer::dumpNodeCounts()
{
	FileBuffer3 out;
	out.open(builder_->workPath() / "node-counts.txt");
	for (int row = 0; row < FastTileCalculator::GRID_EXTENT; row++)
	{
		for (int col = 0; col < FastTileCalculator::GRID_EXTENT; col++)
		{
			uint32_t count = totalNodeCounts_[row * FastTileCalculator::GRID_EXTENT + col];
			if (count > 0)
			{
				out << Tile::fromColumnRowZoom(col, row, FastTileCalculator::ZOOM_LEVEL);
				out << '\t' << count << '\n';
			}
		}
	}
}


void Analyzer::analyze(const char* fileName)
{
	addRequiredStrings();
	read(fileName);

	if(Console::verbosity() >= Console::Verbosity::VERBOSE)
	{
		ConsoleWriter out;
		out.timestamp() << "Analyzed " << totalStats_.nodeCount << " nodes and "
			<< (totalStats_.tagCount * 2 + totalStats_.memberCount) << " strings";
	}

	uint64_t totalStringCount = 0;
	uint64_t totalStringUsageCount = 0;
	StringStatistics::Iterator iter = strings_.iter();
	for (;;)
	{
		const StringStatistics::Counter* counter = iter.next();
		if (!counter) break;
		uint64_t subTotal = counter->trueTotalCount();
		if (subTotal >= 100)
		{
			std::string_view s = counter->stringView();
			totalStringCount++;
			totalStringUsageCount += subTotal;
		}
	}

#ifdef GOL_DIAGNOSTICS
	if (builder_->isDebug()) dumpNodeCounts();

	if(Console::verbosity() >= Console::Verbosity::VERBOSE)
	{
		uint64_t literalsCount = totalStats_.tagCount * 2 + totalStats_.memberCount
			- totalStringUsageCount;

		Console::msg("  %12llu nodes", totalStats_.nodeCount);
		Console::msg("  %12llu ways", totalStats_.wayCount);
		Console::msg("  %12llu relations", totalStats_.relationCount);
		Console::msg("  %12llu members", totalStats_.memberCount);
		Console::msg("  %12llu tags", totalStats_.tagCount);
		Console::msg("  %12llu unique strings in string table", totalStringCount);
		Console::msg("  %12llu unique-string occurrences", totalStringUsageCount);
		Console::msg("  %12llu literal strings", literalsCount);

		Console::msg("Analysis complete.");
	}
#endif
}


// TODO: Need to produce:
// - Global String Table
//   - hard-coded strings come first
//   - then indexed keys
//   - then the most common strings up to #127
//   - then all other keys up to KEY_MAX
//   - finally, remaining keys/values 
//   (make sure not to duplicate)
// - Lookup string -> encoded varint for key & value
// - Lookup of Proto-GOL String Code to GST Code, number or literal string

// - Get all strings & their key counts
// - Sort by key

