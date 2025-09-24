// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <filesystem>
#include "osm/OsmPbfReader.h"
#include "NodeCountTable.h"
#include "build/util/StringStatistics.h"
#include "OsmStatistics.h"

class Analyzer;
class GolBuilder;

class AnalyzerWorker : public OsmPbfContext<AnalyzerWorker, Analyzer>
{
public:
	explicit AnalyzerWorker(Analyzer* analyzer);

	// CRTP overrides
	void stringTable(ByteSpan strings);
	void endBlock(); 
	const uint8_t* node(int64_t id, int32_t lon100nd, int32_t lat100nd, ByteSpan tags);
	void way(int64_t id, ByteSpan keys, ByteSpan values, ByteSpan nodes);
	void relation(int64_t id, ByteSpan keys, ByteSpan values,
		ByteSpan roles, ByteSpan memberIds, ByteSpan memberTypes);
	void afterTasks();
	void harvestResults();
	
private:
	void flush();
	void countString(uint32_t index, int keys, int values);
	int countStrings(ByteSpan strings, int keys, int values);

	struct StringLookupEntry
	{
		uint32_t stringOfs;
		uint32_t counterOfs;
	};


	NodeCountTable nodeCounts_;

	/**
	 * Pointer to the string table of the current block.
	 * (This memory is not owned, it is managed by OsmPbfReader)
	 * This pointer is only valid for the currently processed block.
	 */
	const uint8_t* stringTable_;

	/**
	 * A table that translates an OSM string code to either a string in the
	 * current block's string table (If the string is encountered for the 
	 * first time in this block) or a StringStatistics::Counter.
	 * We use offsets instead of pointers to cut size in half.
	 * We use bit 0 to discriminate the two types:
	 *	Bit 0 = 0: string in string-table
	 *  Bit 0 = 1: StringStatistics::Counter
	 *  Bit 1-31: offset (can address 2 GB)
	 * This approach avoids having to look up the string's Counter more 
	 * than once per OSM block. We could obtain all Counters at the start
	 * of the block (when the OsmPbfReader reads the string table), but this
	 * complicates the flushing mechanism (We may run out of space in the local
	 * arena in the middle of the string table), so we build it lazily instead.
	 */
	std::vector<StringLookupEntry> stringCodeLookup_;
	StringStatistics strings_;
	OsmStatistics stats_;
};

class AnalyzerOutputTask : public OsmPbfOutputTask
{
public:
	AnalyzerOutputTask() {} // TODO: not needed, only to satisfy compiler
	AnalyzerOutputTask(const uint8_t* strings, uint64_t blockBytesProcessed) :
		strings_(strings),
		blockBytesProcessed_(blockBytesProcessed)
	{
	}

	// TODO: Needs to be move-constructable??

	const uint8_t* strings() const { return strings_.get(); }
	uint64_t blockBytesProcessed() const { return blockBytesProcessed_; }

private:
	std::unique_ptr<const uint8_t[]> strings_;
	// size_t currentBatchStringCount_;
	uint64_t blockBytesProcessed_;
};

class Analyzer : public OsmPbfReader<Analyzer, AnalyzerWorker, AnalyzerOutputTask>
{
public:
	explicit Analyzer(GolBuilder* builder);

	uint32_t workerTableSize() const { return 1 * 1024 * 1024; }
	uint32_t workerArenaSize() const { return 2 * 1024 * 1024; }
	uint32_t outputTableSize() const { return 8 * 1024 * 1024; }
	uint32_t outputArenaSize() const { return 64 * 1024 * 1024; }

	void analyze(const char* fileName);
	void startFile(uint64_t size);		// CRTP override
	void processTask(AnalyzerOutputTask& task);
	const FastTileCalculator* tileCalculator() const { return &tileCalculator_; }
	
	OsmStatistics& osmStats() { return totalStats_; }
	const OsmStatistics& osmStats() const { return totalStats_; }
	const StringStatistics& strings() const { return strings_; }
	NodeCountTable& totalNodeCounts() { return totalNodeCounts_; }
	NodeCountTable takeTotalNodeCounts()
	{
		return std::move(totalNodeCounts_);
	}

	void saveNodeCounts(const std::filesystem::path& path) const
	{
		totalNodeCounts_.save(path);
	}

	void saveStringCounts(const std::filesystem::path& path) const
	{
		strings_.save(path);
	}


private:
	void addRequiredStrings();
	void dumpNodeCounts();

	GolBuilder* builder_;
	StringStatistics strings_;
	const FastTileCalculator tileCalculator_;
	int minStringCount_;
	NodeCountTable totalNodeCounts_;
	OsmStatistics totalStats_;
	double workPerByte_;
};
