// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "ParallelQueryPrinter.h"
#include <geodesk/format/WktFormatter.h>

using namespace geodesk;

class WktQueryPrinter : public ParallelQueryPrinter<WktQueryPrinter>
{
public:
    WktQueryPrinter(const QuerySpec* spec) :
        ParallelQueryPrinter(spec)
    {
        // TODO: keyschema
        formatter_.precision(spec->precision());
    }

    void print(FeaturePrinterBuffer& out, FeatureStore* store, FeaturePtr feature) // CRTP override
    {
        out.writeByte(',');
        formatter_.writeFeatureGeometry(out, store, feature);
    }

protected:
     void processBatch(Batch& batch) override
     {
          resultCount_ += batch.count;
          Chunk<char>* chunk = batch.buffers.first();
          if(chunk && chunk->size() > 0)
          {
              if(!prev_.isEmpty())
              {
                  printBatch(std::move(prev_),
                       (first_ && resultCount_ > 1) ? "GEOMETRYCOLLECTION(" : nullptr,
                       first_ ? 1 : 0);
                  first_ = false;
              }
              prev_ = std::move(batch.buffers);
          }
     }

     void printFooter() override
     {
         if(resultCount_ == 0)
         {
             clarisma::ConsoleWriter out;
             out.blank() << "GEOMETRYCOLLECTION EMPTY\n";
             return;
         }
         assert(!prev_.isEmpty());
         printBatch(std::move(prev_),
             (first_ && resultCount_ > 1) ? "GEOMETRYCOLLECTION(" : nullptr,
             first_ ? 1 : 0);
         if(resultCount_ > 1)
         {
             clarisma::ConsoleWriter out;
             out.blank() << ')';
         }
     }

private:
     WktFormatter formatter_;
     ChunkChain<char> prev_;
     bool first_ = true;
};
