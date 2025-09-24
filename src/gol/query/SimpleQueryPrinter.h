// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/cli/ConsoleWriter.h>

#include "QueryPrinter.h"

using namespace geodesk;

class SimpleQueryPrinter : public QueryPrinter
{
protected:
    SimpleQueryPrinter(QuerySpec* spec) :
        QueryPrinter(spec, &SimpleQueryPrinter::consumeResults) {}

    void processBatch(Batch& batch) override;
    virtual void printFeature(FeaturePtr feature) = 0;

private:
    static void consumeResults(QueryBase* query, QueryResults* res);
};
