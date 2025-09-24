// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/alloc/Block.h>
#include <clarisma/io/File.h>
#include <clarisma/thread/TaskEngine.h>
#include <clarisma/util/log.h>
#include <geodesk/feature/Tip.h>
#include <geodesk/geom/Tile.h>

class TileTaskEngine;
namespace geodesk {
class FeatureStore;
}

using namespace clarisma;
using namespace geodesk;

class TileTask
{
public:
	TileTask() {} // TODO: only to satisfy compiler
	TileTask(Tip tip, Tile tile) : tip_(tip), tile_(tile)  {}

	Tip tip() const { return tip_; }
	Tile tile() const { return tile_; }

private:
	Tip tip_;
	Tile tile_;
};


class TileTaskContext
{
public:
	TileTaskContext(TileTaskEngine* engine) : engine_(engine) 
	{
		LOG("Created TileTaskContext -- pointer to engine = %p", engine);
	}
	void processTask(TileTask& task);  // CRTP override
	void afterTasks() {}  // CRTP override
	void harvestResults() {}  // CRTP override

private:
	TileTaskEngine* engine_;
};

class TileOutputTask
{
public:
	TileOutputTask() {} // TODO: only to satisfy compiler
	TileOutputTask(Tip tip, ByteBlock&& data) :
		data_(std::move(data)),
		tip_(tip)
	{}

	Tip tip() const { return tip_; }
	ByteBlock& data() { return data_; }

private:
	ByteBlock data_;
	Tip tip_;
};


class TileTaskEngine : public TaskEngine<TileTaskEngine, TileTaskContext, TileTask, TileOutputTask>
{
public:
	TileTaskEngine(FeatureStore& store, int threadCount);

	void run();
	void processTask(TileOutputTask& task);		// CRTP override
	
	void postOutput(Tip tip, ByteBlock&& data)
	{
		TaskEngine::postOutput(TileOutputTask(tip, std::move(data)));
	}

protected:
	virtual void preProcess() {};
	virtual void prepareTile(Tip tip, Tile tile) {};
	virtual void processTile(Tip tip, Tile tile) {};
	virtual void processOutput(Tip tip, ByteBlock&& data) {};
	// virtual void postProcess() {};
	
	FeatureStore& store() const { return store_; };

private:
	FeatureStore& store_;
	double workPerTile_;
	double workCompleted_;

	friend class TileTaskContext;
};


