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
        if (!processTask(data))
        {
            queue_.clear();
            break;
        }
        ticketsProcessed++;
        if (ticketsProcessed == totalTickets) break;
    }

    coordinator_.setTotalTickets(0);
    for (auto& thread : threads_)
    {
        thread.join();
    }
    return Status::FETCHED_ALL; // TODO: error handling
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
            catch (std::exception& e)
            {
                coordinator_.releaseConnection(client);
                LOGS << "Error: " << e.what();
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
    // TODO: enhanced error handling
    if (data.empty()) return false;

    currentRevision_++;
    updater_.setReadingTask(currentRevision_);

    LOGS << "  Processing " << currentRevision_ << ": "
        << data.size() << " bytes (uncompressed)";
    data[data.size()-1] = static_cast<std::byte>(0);
    // force null-terminator, possibly overwriting > of final closing tag
    try
    {
        ChangeReader reader(updater_.model(), reinterpret_cast<char*>(data.data()));
        reader.read();
    }
    catch (std::exception& e)
    {
        // TODO: set exception as error
        return false;
    }
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