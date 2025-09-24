// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "GolBuilder.h"

#include <clarisma/io/FilePath.h>
#include <clarisma/sys/SystemInfo.h>
#include "build/analyze/Analyzer.h"
#include "build/analyze/TileIndexBuilder.h"
#include "build/sort/Sorter.h"
#include "build/sort/Validator.h"
#include "build/compile/Compiler.h"
#ifdef GEODESK_PYTHON
#include "python/util/util.h"
#endif

GolBuilder::GolBuilder() :
	threadCount_(0),
	workCompleted_(0)
{
}

// Resources we need:
// - A lookup from coordinates to tiles



void GolBuilder::build(const char* golPath, int startPhase)
{
	int cores = std::thread::hardware_concurrency();
	threadCount_ = settings_.threadCount();
	if (threadCount_ == 0)
	{
		threadCount_ = cores;
	}
	else if (threadCount_ > 4 * cores)
	{
		threadCount_ = 4 * cores;
	}

	console().start("Analyzing...");
	// SystemInfo sysinfo;
	calculateWork();

	std::string strGolPath = FilePath::withDefaultExtension(golPath, ".gol");
	golPath_ = strGolPath;
	std::string_view withoutExt = FilePath::withoutExtension(strGolPath);
	workPath_ = Strings::combine(withoutExt, "-work");
	std::filesystem::create_directories(workPath_);
	if (settings_.keepIndexes())
	{
		indexPath_ = Strings::combine(withoutExt, "-indexes");
		std::filesystem::create_directories(indexPath_);
	}
	else
	{
		indexPath_ = workPath_;
	}

	analyze(startPhase <= ANALYZE);
	if (startPhase <= SORT)
	{
		prepare();
	}
	else
	{
		std::string path = (workPath_ / "features.bin").string();
		featurePiles_.openExisting(path.c_str());
	}
	if (startPhase <= SORT) sort();
	if (startPhase <= VALIDATE) validate();
	compile();

	if(indexFinalizerThread_.joinable()) indexFinalizerThread_.join();
		// we have to wait for the indexes to be released and closed
		// before we can delete them
	if (Console::verbosity() < Console::Verbosity::DEBUG)
	{
		featurePiles_.clear();
		featurePiles_.close();
		std::error_code error;
		std::filesystem::remove_all(workPath_, error);
	}
}

void GolBuilder::analyze(bool full)
{
	NodeCountTable nodeCounts;
	if (full)
	{
		Analyzer analyzer(this);
		analyzer.analyze(settings_.sourcePath().c_str());
		stats_ = analyzer.osmStats();
		metadata_ = analyzer.metadata();

		if (debug_)
		{
			analyzer.saveNodeCounts(workPath() / "node-counts.bin");
			analyzer.saveStringCounts(workPath() / "string-counts.bin");
		}
		Console::get()->setTask("Preparing indexes...");
		nodeCounts = analyzer.takeTotalNodeCounts();

		// TODO: Caution! Order of strings may change across multiple
		//  invocations if there is a tie among string counts
		stringCatalog_.build(settings_, analyzer.strings().span());
	}
	else
	{
		Console::get()->setTask("Preparing indexes...");
		nodeCounts.load(workPath() / "node-counts.bin");
		ByteBlock strings = File::readAll(workPath() / "string-counts.bin");
		stringCatalog_.build(settings_, strings);
	}

	TileIndexBuilder tib(settings_);
	tib.build(std::move(nodeCounts));
	tileIndex_ = tib.takeTileIndex();
	tileSizeEstimates_ = tib.takeTileSizeEstimates();

#ifdef GOL_DIAGNOSTICS
	if(Console::verbosity() >= Console::Verbosity::VERBOSE)
	{
		Console::msg("Building tile lookup...");
	}
#endif

	tileCatalog_.build(tib);
	tileCatalog_.write(workPath_ / "tile-catalog.txt");

#ifdef GOL_DIAGNOSTICS
	if(Console::verbosity() >= Console::Verbosity::VERBOSE)
	{
		Console::msg("Tile lookup built.");
	}
#endif
}


void GolBuilder::createIndex(MappedIndex& index, const char* name, int64_t maxId, int extraBits)
{
	int bits = 32 - Bits::countLeadingZerosInNonZero32(tileCatalog_.tileCount());
		// The above is correct; if we have 512 tiles, we need to store 513
		// distinct values (pile number starts at 1, 0 = "missing")
		// Hence, it's not enough to have 9 bits, but we will need 10
		// 0x200 (decimal 512) has 22 leading zeroes --> 10 bits
	std::string path = (indexPath_ / name).string();
	index.create(path.c_str(), maxId, bits + extraBits);
}

void GolBuilder::prepare()
{
	createIndex(featureIndexes_[0], "nodes.idx", stats_.maxNodeId, 0);
	createIndex(featureIndexes_[1], "ways.idx", stats_.maxWayId, 2);
	createIndex(featureIndexes_[2], "relations.idx", stats_.maxRelationId, 2);

	// TODO: Decide whether tileSizeEstimates_ is 0-based or 1-based

	int tileCount = tileCatalog_.tileCount();
	std::string pileFilePath = (workPath_ / "features.bin").string();
	featurePiles_.create(pileFilePath.c_str(),
		tileCount, 64 * 1024, tileSizeEstimates_[0]);

	for (int i = 1; i <= tileCount; i++)
	{
		featurePiles_.preallocate(i, tileSizeEstimates_[i]);
	}
}


void GolBuilder::sort()
{
	Sorter sorter(this);
	sorter.sort(settings_.sourcePath().c_str());

	// Console::get()->setTask("Clearing indexes...");
	indexFinalizerThread_ = std::thread(&GolBuilder::finalizeIndexes, this);
}


void GolBuilder::finalizeIndexes()
{
	for(auto& index : featureIndexes_)
	{
		if(settings_.keepIndexes())
		{
			index.sync();
			index.release();
		}
		else
		{
			//Console::debug("Clearing index...");
			index.clear();
			// Console::debug("  Done.");
		}
		index.close();
	}
}

void GolBuilder::validate()
{
	Validator validator(this);
	validator.validate();
}

void GolBuilder::compile()
{
	Compiler compiler(this);
	compiler.compile();
}

#ifdef GEODESK_PYTHON

PyObject* GolBuilder::build(PyObject* args, PyObject* kwds)
{
	GolBuilder builder;
	PyObject* arg = PyTuple_GetItem(args, 0);
	const char* golFile = PyUnicode_AsUTF8(arg);
	if (!golFile) return NULL;
	if (builder.setOptions(kwds) < 0) return NULL;
	builder.build(golFile);
	Py_RETURN_NONE; // TODO
}

#endif

void GolBuilder::calculateWork()
{
	workPerPhase_[ANALYZE]  = 10.0;
	workPerPhase_[SORT]     = 40.0;
	workPerPhase_[VALIDATE] = 20.0;
	workPerPhase_[COMPILE]  = 30.0;
}

