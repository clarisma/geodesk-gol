// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/data/Chunk.h>
#include <clarisma/thread/TaskQueue.h>
#include <geodesk/query/QueryBase.h>
#include "QuerySpec.h"

using namespace geodesk;

class QueryPrinter : public QueryBase
{
public:
    QueryPrinter(const QuerySpec* spec, QueryResultsConsumer consumer);
    virtual ~QueryPrinter() = default;

    void setProgressScope(double start, double length)
    {
        progressStart_ = start;
        progressPortion_ = length;
    }
    int64_t run();
    int64_t resultCount() const { return resultCount_; }
    void submitResults(const Box& bounds,
        clarisma::ChunkChain<char>&& buffers,
        QueryResults* results, int count, bool complete);

protected:
    class Batch
    {
    public:
        Batch() {}       // needed to satisfy compiler
        explicit Batch(const Box& bounds_,
            clarisma::ChunkChain<char>&& buffers_,
            QueryResults* results_, int count_, bool complete_) :
            count(count_),
            complete(complete_),
            buffers(std::move(buffers_)),
            results(results_),
            bounds(bounds_)
        {
        }

        int count;
        bool complete;
        clarisma::ChunkChain<char> buffers;
        QueryResults* results;
        Box bounds;
    };

    const QuerySpec* spec() const { return spec_; }

    virtual void printHeader() {}
    virtual void printFooter() {}
    virtual void processBatch(Batch& batch) = 0;

    int64_t resultCount_;

private:
    void process();
    void processTask(Batch& task);

    const QuerySpec* spec_;
    // Keep this order, as thread_ depends on queue_
    clarisma::TaskQueue<QueryPrinter,Batch> queue_;
    std::thread thread_;
    std::atomic<int> totalTiles_;
    int tilesProcessed_;
    double progressStart_ = 0;
    double progressPortion_ = 100;

    friend class clarisma::TaskQueue<QueryPrinter,Batch>;
};
