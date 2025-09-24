// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "ParallelQueryPrinter.h"
#include <geodesk/format/GeoJsonFormatter.h>

using namespace geodesk;

class GeoJsonQueryPrinter : public ParallelQueryPrinter<GeoJsonQueryPrinter>
{
public:
    GeoJsonQueryPrinter(const QuerySpec* spec, bool linewise) :
        ParallelQueryPrinter(spec),
        linewise_(linewise),
        isFirstBatch_(!linewise)
    {
        // TODO: keyschema
        formatter_.precision(spec->precision());
    }

    void print(FeaturePrinterBuffer& out, FeatureStore* store, FeaturePtr feature) // CRTP override
    {
        if (!linewise_) out.writeByte(',');
        formatter_.writeFeature(out, store, feature);
        if (linewise_) out.writeByte('\n');
    }

private:
    void processBatch(Batch& batch) override
    {
        Chunk<char>* chunk = batch.buffers.first();
        if(chunk && chunk->size() > 0)
        {
            printBatch(std::move(batch.buffers), nullptr, isFirstBatch_ ? 1 : 0);
            isFirstBatch_ = false;
            // TODO: pretty GeoJSON has different formatting needs!
        }
        resultCount_ += batch.count;
    }

    void printHeader() override
    {
        if (linewise_) return;  // No header for GeoJSONL
        clarisma::ConsoleWriter out;
        out.blank() << "{\"type\":\"FeatureCollection\",\"generator\":\"geodesk-gol/"
            "2.0.0" << "\",\"features\":[";
        // TODO: geodesk-gol/version
        // TODO: pretty
    }

    void printFooter() override
    {
        if (linewise_) return;  // No footer for GeoJSONL
        clarisma::ConsoleWriter out;
        out.blank() << "]}";
        // TODO: pretty
    }

    GeoJsonFormatter formatter_;
    bool linewise_ = false;
    bool isFirstBatch_ = true;
};
