// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "QueryPrinter.h"
#include <clarisma/cli/Console.h>
#include <clarisma/data/Chunk.h>

using namespace clarisma;

QueryPrinter::QueryPrinter(const QuerySpec* spec, QueryResultsConsumer consumer) :
    QueryBase(spec->store(), spec->box(), spec->types(),
        spec->matcher(), spec->filter(), consumer),
    resultCount_(0),
    spec_(spec),
    queue_(1024),     // TODO !!!
    thread_(&QueryPrinter::process, this),
    totalTiles_(spec->store()->tileCount()),
        // must set this to a high value so procesTask does not
        // quit prematurely if we haven't established the true
        // tile count yet
    tilesProcessed_(0)
{
}

// TODO: This could deadlock if all tiles are processed before the
//  true totalTiles_ is set (extremely unlikely, but possible)
//  possible option is to set totalTiles_ + 1, and submit an empty
//  result at end, so the queue is woken up
//  Or: hold off on submitting final task until we've set the true tile count

// TODO: what happens if there are no tiles at all (e.g. all stale)?

int64_t QueryPrinter::run()
{
    int submitCount = store_->executor().minimumRemainingCapacity();
    bool hasMore = true;
    int trueTileCount = 0;

    while(submitCount > 0)
    {
        if(tileIndexWalker_.currentEntry().isLoadedAndCurrent()) [[likely]]
        {
            TileQueryTask task(this,
                (tileIndexWalker_.currentTip() << 8) |
                tileIndexWalker_.northwestFlags(),
                FastFilterHint(tileIndexWalker_.turboFlags(), tileIndexWalker_.currentTile()));
            store_->executor().post(task);
            trueTileCount++;
            submitCount--;
        }
        hasMore = tileIndexWalker_.next();
        if(!hasMore) break;
    }
    if(hasMore)
    {
        struct QueryTile
        {
            uint32_t tipAndFlags;
            Tile tile;
        };

        int maxRemaining = store_->tileCount() - trueTileCount;
        std::unique_ptr<QueryTile[]> tiles(new QueryTile[maxRemaining]);
        QueryTile* p = tiles.get();
        for(;;)
        {
            if(tileIndexWalker_.currentEntry().isLoadedAndCurrent()) [[likely]]
            {
                p->tipAndFlags =
                    (tileIndexWalker_.currentTip() << 8) |
                    tileIndexWalker_.northwestFlags() |
                    (tileIndexWalker_.turboFlags() & 1);
                p->tile = tileIndexWalker_.currentTile();
                p++;        // TODO: guard against overrun
                trueTileCount++;
            }
            if(!tileIndexWalker_.next()) break;
        }
        totalTiles_ = trueTileCount;

        const QueryTile *pEnd = p;
        p = tiles.get();
        while(p < pEnd)
        {
            TileQueryTask task(this, p->tipAndFlags & ~1,
                FastFilterHint(p->tipAndFlags & 1, p->tile));
            store_->executor().post(task);
            p++;
            // LOGS << "Submitted request for tile " << (p->tipAndFlags >> 8) << "\n";
        }
    }
    else
    {
        totalTiles_ = trueTileCount;

        // TODO: may need to send an empty task to get the queue
        //  to wake up if it processed all previous tasks
    }
    LOGS << "Submitted all tiles.\n";

    if(thread_.joinable()) thread_.join();

    LOGS << "Done.\n";
    return resultCount_;
}

void QueryPrinter::process()
{
    printHeader();
    queue_.process(this);
    // TODO: add post-process (or merge with printFooter?)
    //  needed for XmlPrinter
    LOGS << "Before printFooter()";
    printFooter();
    LOGS << "After printFooter()";
}

void QueryPrinter::processTask(Batch& task)
{
    tilesProcessed_ += task.complete;
    processBatch(task);

    Console::get()->setProgress(static_cast<int>(
        progressStart_ +
        static_cast<double>(tilesProcessed_) * progressPortion_ / totalTiles_));
    if(tilesProcessed_ >= totalTiles_)
    {
        LOGS << "Processed " << tilesProcessed_ << " tiles, "
            << resultCount_ << " results.\n";
        LOGS << "Shutting down result queue...\n";
        queue_.shutdown();
    }
}


void QueryPrinter::submitResults(const Box& bounds,
    ChunkChain<char>&& buffers,
    QueryResults* results, int count, bool complete)
{
    queue_.post(Batch(bounds, std::move(buffers), results, count, complete));
}
