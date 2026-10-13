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

class ChangeIngester
{
public:
    ChangeIngester(Updater& updater, std::string_view url,
        int maxConnections);

    void download(uint32_t maxThreads);

private:
    static constexpr int QUEUE_SIZE = 8;
    static constexpr uint32_t MAX_THREADS = 8;
    static constexpr int MAX_RETRIES = 3;
    static constexpr int INITIAL_RETRY_DELAY = 250;

    void performDownload();
    bool processTask(std::vector<std::byte>& data);

    void formatPath(char* path, uint32_t ticket, size_t maxPath) const;

    Updater& updater_;
    DownloadCoordinator coordinator_;
    SequencedBlockingQueue<std::vector<std::byte>> queue_;
    std::string_view url_;
    StaticVector<std::thread, MAX_THREADS> threads_;
    size_t bufferSize_ = 0;
    uint32_t baseRevision_ = 0;
    uint32_t currentRevision_ = 0;
    uint32_t desiredTargetRevision_ = 0;
    uint32_t targetRevision_ = 0;
};
