// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "DownloadCoordinator.h"
#include <string_view>
#include <thread>
#include <clarisma/data/StaticVector.h>
#include <clarisma/thread/SequencedBlockingQueue.h>

class Updater;

using namespace clarisma;

class ChangeIngester2
{
public:

private:
    static constexpr int QUEUE_SIZE = 8;
    static constexpr int MAX_THREADS = 8;

    void performDownload();

    Updater& updater_;
    DownloadCoordinator coordinator_;
    SequencedBlockingQueue<std::vector<std::byte>> queue_;
    std::string_view url_;
    StaticVector<std::thread, MAX_THREADS> threads_;
    size_t bufferSize_ = 0;
    uint32_t currentRevision_ = 0;
    uint32_t desiredTargetRevision_ = 0;
    uint32_t targetRevision_ = 0;
};
