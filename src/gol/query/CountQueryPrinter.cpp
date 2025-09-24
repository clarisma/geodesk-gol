// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "CountQueryPrinter.h"

void CountQueryPrinter::consumeResults(QueryBase* query, QueryResults* res)
{
    CountQueryPrinter* printer = static_cast<CountQueryPrinter*>(query);
    int count = 0;
    if(res != QueryResults::EMPTY)
    {
        QueryResults* first = res;
        do
        {
            QueryResults* next = res->next;
            count += res->count;
            delete res;
            res = next;
        }
        while(res != first);
    }
    printer->submitResults(Box(), clarisma::ChunkChain<char>(),
        QueryResults::EMPTY, count, true);
}


void CountQueryPrinter::processBatch(Batch& batch)
{
    assert(batch.buffers.isEmpty());
    assert(batch.results == QueryResults::EMPTY);
    resultCount_ += batch.count;
}


void CountQueryPrinter::printFooter()
{
    clarisma::ConsoleWriter out;
    out.blank() << resultCount_ << '\n';
}