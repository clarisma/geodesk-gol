// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangeIngester2.h"
#include "ChangeReader.h"
#include "Replication.h"
#include "clarisma/thread/Threads.h"
#include "update/Updater.h"

#define GEODESK_GOL_UPDATE_INGESTER2
#ifdef GEODESK_GOL_UPDATE_INGESTER2

// Common errors:
// - No connection, timeout, etc --> retry
// - 404 for state.txt: Might not be a replication endpoint
// - 404 for .osc.gz: Revision x is missing


ChangeIngester::ChangeIngester(Updater& updater, std::string_view url,
    int maxConnections) :
    updater_(updater),
    coordinator_(url, "geodesk-gol/" GEODESK_GOL_VERSION " (+https://www.geodesk.com)", maxConnections),
    // TODO: Put UserAgent string in one common place)
    queue_(QUEUE_SIZE),
    url_(url)
{
}

ChangeIngester::Status ChangeIngester::download(uint32_t maxThreads)
{
    FeatureStore* store = updater_.model().store();

    HttpClient* client = coordinator_.createInitialConnectionUnlocked();
    Replication::State availableTarget = Replication::fetchState(*client);

    Replication::State current;
    if (url_ == store->replicationUrl())
    {
        // If URL is same as used for previous update,
        // use the current revision
        current.revision = store->revision();
        current.timestamp = store->revisionTimestamp();
    }
    else
    {
        // Otherwise, determine the current revision based on timestamp
        current = Replication::findCurrentState(*client,
            store->revisionTimestamp(), availableTarget);
    }

    // TODO
    desiredTargetRevision_ = availableTarget.revision;
    currentTimestamp_ = availableTarget.timestamp;

    if (current.revision >= desiredTargetRevision_)
    {
        // TODO: already current
        return Status::NO_UPDATES;
    }

    baseRevision_ = current.revision;
    currentRevision_ = baseRevision_;

    updater_.beginUpdate(baseRevision_, current.timestamp,
        availableTarget.revision, availableTarget.timestamp);
        // TODO

    uint32_t totalTickets = desiredTargetRevision_ - current.revision;
    client = nullptr;
        // We don't use client again once we start the worker
        // threads; if we need to perform another check, we have
        // to acquire a connection again via the coordinator

    coordinator_.setTotalTickets(totalTickets);

    if (maxThreads == 0)
    {
        maxThreads = std::thread::hardware_concurrency();
    }
    uint32_t threadCount = std::min(totalTickets, maxThreads);
    threadCount = std::min(threadCount, MAX_THREADS);

    for (uint32_t i = 0; i < threadCount; i++)
    {
        threads_.emplace_back(&ChangeIngester::performDownload, this);
    }

    uint32_t ticketsProcessed = 0;
    for (;;)
    {
        std::vector<std::byte> data = queue_.take();
        ticketsProcessed++;
        try
        {
            if (!processTask(data)) break;
        }
        catch (std::exception& ex)
        {
            error_ = ex.what();
            currentRevision_ = baseRevision_;
            break;
        }
        if (ticketsProcessed == totalTickets) break;
    }

    Status status;
    uint32_t ticketsIssued = coordinator_.stop();
    assert(ticketsProcessed <= ticketsIssued);

    if (currentRevision_ == desiredTargetRevision_) [[likely]]
    {
        status = Status::FETCHED_ALL;
    }
    else if (currentRevision_ > baseRevision_)
    {
        status = Status::FETCHED_SOME_ERRORS;
    }
    else
    {
        status = Status::FAILED;
    }

    while (ticketsProcessed < ticketsIssued)
    {
        // Drain the queue to unblock any workers that
        // are waiting to submit later osc files after
        // we've quit processing due to an error
        queue_.take();
        ticketsProcessed++;
    }
    for (auto& thread : threads_)
    {
        thread.join();
    }
    return status;
}


void ChangeIngester::performDownload()
{
    std::vector<std::byte> data;

    for (;;)
    {
        auto [client, ticket ] = coordinator_.takeConnectionAndTicket();
        if (!client) break;

        char path[256];
        formatPath(path, ticket, sizeof(path));
        LOGS << Threads::currentThreadId() << ": Downloading " << path;

        int attempt = 0;
        for (;;)
        {
            try
            {
                client->getUnzippedGzip(path, data);
            }
            catch (std::exception& ex)
            {
                coordinator_.releaseConnection(client);
                std::string_view error = ex.what();
                LOGS << "Error: " << error;
                data.clear();
                if (attempt < MAX_RETRIES)
                {
                    int ms = INITIAL_RETRY_DELAY << attempt;
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(ms));
                    attempt++;
                    LOGS << "  Retry attempt " << attempt;
                    client = coordinator_.takeConnection();
                    if (client) continue;
                        // If we got a nullptr client, that
                        // means downloading is shutting down,
                        // and we give up
                }
                // TODO: enqueue error instead of data to
                //  signal that ingestion failed
                LOGS << "Failed, giving up";
                data.resize(error.size() + 2);
                data[0] = ERROR_MARKER;
                memcpy(&data[1], error.data(), error.size() + 1);
                queue_.put(ticket, std::move(data));
                    // TODO: for now, post empty vector to signal end
                return;
            }
            coordinator_.releaseConnection(client);
            queue_.put(ticket, std::move(data));
            break;
        }
    }
}


bool ChangeIngester::processTask(std::vector<std::byte>& data)
{
    if (data.empty()) [[unlikely]]
    {
        return false;
    }
    if (data[0] == ERROR_MARKER)  [[unlikely]]
    {
        error_ = reinterpret_cast<const char*>(&data[1]);
        return false;
    }

    currentRevision_++;
    updater_.setReadingTask(currentRevision_);

    LOGS << "  Processing " << currentRevision_ << ": "
        << data.size() << " bytes (uncompressed)";
    data[data.size()-1] = static_cast<std::byte>(0);
    // force null-terminator, possibly overwriting > of final closing tag

    // For now, we don't catch exceptions cause due to an invalid ocs file;
    // we let these bubble up and abort the entire update
    // If we caught them and stopped ingest (as we do with transport
    // errors or gzip-related errors), we would wind up with a model
    // that may contain parts of the invalid osc file, which violates
    // our atomicity guarantee

    ChangeReader reader(updater_.model(), reinterpret_cast<char*>(data.data()));
    reader.read();

    updater_.reportFileRead(data.size());
    return true;
}


void ChangeIngester::formatPath(char* path, uint32_t ticket, size_t maxPath) const
{
    // TODO
    char *p = Replication::formatPath(path, baseRevision_ + ticket + 1);
    memcpy(p, ".osc.gz", 8);
}

#endif