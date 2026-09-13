// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <string_view>
#include <thread>
#include <clarisma/thread/BlockingQueue.h>

class Updater;

using namespace clarisma;

class ChangeIngester
{
public:
    enum class Status
    {
        NO_UPDATES,
        PARTIALLY_FETCHED,
        FULLY_FETCHED
    };

    explicit ChangeIngester(Updater& updater);

    void download(std::string_view url);
    std::string_view error() const { return error_; }
    Status status() const { return status_; }

private:
    void performDownload();
    void processTask(std::vector<std::byte>& data);

    static constexpr int QUEUE_SIZE = 8;

    Updater& updater_;
    BlockingQueue<std::vector<std::byte>> queue_;
    std::string_view url_;
    std::thread thread_;
    size_t bufferSize_ = 0;
    uint32_t currentRevision_ = 0;
    Status status_ = Status::NO_UPDATES;
    std::atomic<bool> stopFetching_ = false;
    std::string error_;
};
