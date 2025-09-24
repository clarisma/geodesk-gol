// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/util/ChunkBuffer.h>
#include <geodesk/feature/NodePtr.h>
#include "QueryPrinter.h"

using namespace geodesk;
using clarisma::Chunk;
using clarisma::ChunkChain;

class FeaturePrinterBuffer : public clarisma::ChunkBuffer
{
public:
    explicit FeaturePrinterBuffer(QueryPrinter* printer, size_t size = 64 * 1024) :
        ChunkBuffer(size),
        printer_(printer),
        featureStart_(nullptr),
        count_(0)
    {
    }

    void markFeatureStart() { featureStart_ = p_; }
    void addBounds(FeaturePtr feature)
    {
        if(feature.isNode())
        {
            NodePtr node(feature);
            bounds_.expandToInclude(node.xy());
        }
        else
        {
            bounds_.expandToIncludeSimple(feature.bounds());
        }
    }
    void addCount(int count) { count_ += count; }

    void filled(char* p) override
    {
        assert(p >= buf_);
        assert(p <= end_);
        Chunk<char>* chunk = Chunk<char>::ptrFromData(buf_);
        size_t capacity = chunk->size();
        if(featureStart_ > buf_)
        {
            size_t trueSize = featureStart_ - buf_;
            ChunkChain chain = takeAndReplace(capacity);    // changes buf_ to new chunk
            size_t toCopy = p - featureStart_;
            memcpy(buf_, featureStart_, toCopy);
            featureStart_ = buf_;
            p_ = buf_ + toCopy;
            chunk->trim(trueSize);
            // LOGS << "Submitting full chunk (" << trueSize << " bytes)";
            submit(std::move(chain), false);
        }
        else
        {
            Chunk<char>* newChunk = Chunk<char>::create(capacity);
            chunk->trim(p - buf_);
            chunk->setNext(newChunk);
            useChunk(newChunk);
            featureStart_ = buf_;
                // Not technically true, but it forces the Buffer to
                // keep adding chunks to the chain because the Buffer
                // holds a single feature
            LOGS << "Adding chunk to chain";
        }
    }

    void flush(char* p) override
    {
        assert(p >= buf_);
        assert(p <= end_);
        Chunk<char>* chunk = Chunk<char>::ptrFromData(buf_);
        chunk->trim(p - buf_);
        submit(take(), true);
        featureStart_ = nullptr;
    }

    void flush()
    {
        flush(p_);
    }

private:
    void submit(ChunkChain<char>&& chain, bool completed)
    {
        #ifndef NDEBUG
        /*
        size_t contentSize = chain.calculateTotalSize();
        if(contentSize > 0 && count_ == 0)
        {
            LOGS << contentSize << " bytes of content, but count==0";
        }
        if(contentSize == 0 && count_ > 0)
        {
            LOGS << "0 bytes of content, but count==" << count_;
        }
        */
        #endif
        /*
        assert((chain.calculateTotalSize() == 0 && count_ ==0) ||
            (chain.calculateTotalSize() > 0 && count_  > 0));
        */
        printer_->submitResults(bounds_, std::move(chain),
            QueryResults::EMPTY, count_, completed);
        count_ = 0;
        // no need to reset bounds_, since it is cumulative
    }
    QueryPrinter* printer_;
    Box bounds_;
    const char* featureStart_;
    int count_;
};

