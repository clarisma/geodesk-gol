// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileSizeCollector.h"

using namespace clarisma;

void TileSizeCollector::collect()
{
    constexpr uint32_t THREAD_COUNT = 16;

    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);

    for (int i=0; i < THREAD_COUNT; i++)
    {
        threads.emplace_back(&TileSizeCollector::worker, this);
    }

    for (auto &thread : threads) thread.join();
}

void TileSizeCollector::worker()
{
    constexpr size_t CHUNK_SIZE = 64;
    for (;;)
    {
        int begin = cursor.fetch_add(CHUNK_SIZE);
        if (begin >= tiles_.size()) break;
        int end = std::min(begin + CHUNK_SIZE, tiles_.size());
        for (int i = begin; i < end; i++)
        {
            // TODO
        }
    }
}