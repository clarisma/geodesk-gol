// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "SimpleQueryPrinter.h"

using namespace clarisma;

void SimpleQueryPrinter::processBatch(Batch& batch)
{
    QueryResults* res = batch.results;
    if(res != QueryResults::EMPTY)
    {
        QueryResults* first = res;
        do
        {
            for (FeaturePtr feature : *res)
            {
                printFeature(feature);
                resultCount_++;
            }
            QueryResults* next = res->next;
            delete res;
            res = next;
        }
        while(res != first);
    }
}

void SimpleQueryPrinter::consumeResults(QueryBase* query, QueryResults* res)
{
    static_cast<SimpleQueryPrinter*>(query)->submitResults(
    Box(), clarisma::ChunkChain<char>(), res, 0, true);
}
