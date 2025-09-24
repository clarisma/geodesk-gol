// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/cli/ConsoleWriter.h>
#include <clarisma/util/ChunkBuffer.h>
// #include <clarisma/util/log.h>
#include <geodesk/feature/NodePtr.h>
#include "QueryPrinter.h"
#include "FeaturePrinterBuffer.h"

template<typename Derived>
class ParallelQueryPrinter : public QueryPrinter
{
public:
    explicit ParallelQueryPrinter(const QuerySpec* spec) :
        QueryPrinter(spec, &Derived::consumeResults)
    {
    }

    Derived* self() const { return static_cast<Derived*>(this); }

    /// Prints the given chain of buffers to stdout, optionally
    /// printing a prefix and skipping the characters of the first buffer.
    ///
    /// @param buffers the chain of buffers (must contain at least one)
    /// @param prefix the content to write before the buffers (or nullptr)
    /// @param skip the number of characters to skip at the start of the
    ///     first buffer (must not exceed the buffer size)
    ///
    static void printBatch(ChunkChain<char>&& buffers, const char* prefix, int skip)
    {
        Chunk<char>* chunk = buffers.first();
        if(!chunk || skip >= chunk->size()) return;

        clarisma::ConsoleWriter out;
        out.blank();
        if(prefix) out << prefix;
        int chunkCount = 0;
        do
        {
            out << std::string_view(chunk->data() + skip, chunk->size() - skip);
            skip = 0;
            chunk = chunk->next();
            chunkCount++;
        }
        while(chunk);
    }


protected:
    void processBatch(Batch& batch) override
    {
        // assert(batch.count > 0);
        printBatch(std::move(batch.buffers), nullptr, 0);
        resultCount_ += batch.count;
    }

    static void consumeResults(QueryBase* query, QueryResults* res)
    {
        Derived* self = static_cast<Derived*>(query);
        if(res == QueryResults::EMPTY)
        {
            self->submitResults(Box(), ChunkChain<char>(), res, 0, true);
            return;
        }

        FeaturePrinterBuffer buf(self);
        QueryResults* first = res;
        do
        {
            buf.addCount(res->count);
            for(int i=0; i<res->count; i++)
            {
                FeaturePtr feature(res->pTile + res->items[i]);
                buf.markFeatureStart();
                buf.addBounds(feature);
                self->print(buf, self->store(), feature);
            }
            res = res->next;
        }
        while(res != first);
        buf.flush();
    }

    void print(FeaturePrinterBuffer& out, FeatureStore* store, FeaturePtr feature); // CRTP override
};
