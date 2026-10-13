// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangeIngester2.h"
#include "ChangeReader.h"
#include "Replication.h"
#include "update/Updater.h"

ChangeIngester::ChangeIngester(Updater& updater, std::string_view url,
    int maxConnections) :
    updater_(updater),
    coordinator_(url, "geodesk-gol/" GEODESK_GOL_VERSION " (+https://www.geodesk.com)", maxConnections),
    // TODO: Put UserAgent string in one common place)
    queue_(QUEUE_SIZE)
{
}

void ChangeIngester::download(uint32_t maxThreads)
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
        return;
    }

    baseRevision_ = current.revision;
    currentRevision_ = baseRevision_;

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
        if (!processTask(data)) break;
        ticketsProcessed++;
    }

    coordinator_.setTotalTickets(0);
    for (auto& thread : threads_)
    {
        thread.join();
    }
}



void ChangeIngester::performDownload()
{
    std::vector<std::byte> data;

    for (;;)
    {
        auto [client, ticket ] = coordinator_.takeConnectionAndTicket();
        if (!client) break;

        int attempt = 0;
        for (;;)
        {
            char path[256];
            formatPath(path, ticket, sizeof(path));
            try
            {
                client->getUnzippedGzip(path, data);
            }
            catch (std::exception& e)
            {
                data.clear();
                if (attempt < MAX_RETRIES)
                {
                    int ms = INITIAL_RETRY_DELAY << attempt;
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(ms));
                    attempt++;
                    continue;
                }
                // TODO: enqueue error instead of data to
                //  signal that ingestion failed
                queue_.put(ticket, std::move(data));
                    // TODO: for now, post empty vector to signal end
                return;
            }
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
    ChangeReader reader(updater_.model(), reinterpret_cast<char*>(data.data()));

    //assert(_CrtCheckMemory());
    reader.read();
    // updater_.reportFileRead(osc.size());
    updater_.reportFileRead(data.size());
    return true; // TODO: exceptions
}


void ChangeIngester::formatPath(char* path, uint32_t ticket, size_t maxPath) const
{
    // TODO
    Replication::formatPath(path, baseRevision_ + ticket + 1);
}
