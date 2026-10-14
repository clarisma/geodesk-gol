// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "DownloadCoordinator.h"
#include <string_view>
#include <thread>
#include <clarisma/data/StaticVector.h>
#include <clarisma/thread/SequencedBlockingQueue.h>

#include "clarisma/util/DateTime.h"

class Updater;

using namespace clarisma;

class ChangeIngester
{
public:
    ChangeIngester(Updater& updater, std::string_view url,
        int maxConnections);

    enum class Status
    {
        NO_UPDATES,
        FETCHED_ALL,
        FETCHED_SOME_ERRORS,
        FETCHED_SOME_BUFFER_FULL,
        FAILED
    };

    Status download(uint32_t maxThreads);
    std::string_view error() const { return error_; }
    uint32_t latestRevision() const { return latestRevision_; }
    DateTime latestTimestamp() const { return latestTimestamp_; }
        // TODO: define semantics

private:
    static constexpr int QUEUE_SIZE = 8;
    static constexpr uint32_t MAX_THREADS = 8;
    static constexpr int MAX_RETRIES = 3;
    static constexpr int INITIAL_RETRY_DELAY = 250;

    static constexpr std::byte ERROR_MARKER = static_cast<std::byte>('~');

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
    uint32_t latestRevision_ = 0;
    uint32_t desiredTargetRevision_ = 0;
    uint32_t targetRevision_ = 0;
    DateTime latestTimestamp_;
    std::string error_;
};
