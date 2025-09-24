// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "QueryPrinter.h"

using namespace geodesk;

class CountQueryPrinter : public QueryPrinter
{
public:
    CountQueryPrinter(const QuerySpec* spec) :
    QueryPrinter(spec, &CountQueryPrinter::consumeResults) {}

protected:
    void processBatch(Batch& batch) override;
    void printFooter() override;

private:
    static void consumeResults(QueryBase* query, QueryResults* res);
};
