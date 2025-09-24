// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangeIngester.h"

#include <clarisma/zip/Zip.h>
#include "Updater.h"
#include "ChangeReader.h"
#include "ReplicationClient.h"
#include "model/ChangeModel.h"


// TODO: Make sure to set another task before this class is destroyed,
//  because we use a pointer to a buffer in this class for setTask

ChangeIngester::ChangeIngester(Updater& updater) :
    updater_(updater),
    queue_(QUEUE_SIZE)
{
}


// check for updates
// if server_revision == client_revision:
//     Do nothing; all up to date
// seq = client_revision
// do:
//    seq++
//    download changes
//    unpack and dispatch to main thread
// while space in buffer


void ChangeIngester::download(std::string_view url)
{
    url_ = url;
    thread_ = std::thread(&ChangeIngester::performDownload, this);
    LOGS << "Starting queue to process downloaded files";
    try
    {
        for (;;)
        {
            std::vector<std::byte> data = queue_.take();
            if (data.empty()) break;
            processTask(data);
        }
    }
    catch (std::exception& ex)
    {
        LOGS << "Processing change file failed: " << ex.what();
        stopFetching_ = true;
        queue_.clear();
        thread_.join();
        throw;
    }
    if (thread_.joinable()) thread_.join();
    LOGS << "All changes ingested";
}


// TODO: Need to be able to cancel downloads if an error occurs during
//  processing of files

void ChangeIngester::performDownload()
{
    std::vector<std::byte> data;
    try
    {
        ReplicationClient client(url_);
        ReplicationClient::State target = client.fetchState();
        FeatureStore* store = updater_.store();

        LOGS << "Latest revision on server: " << target.revision;

        // TODO: If URL is same as used for previous update,
        //  can you use the current revision
        // uint32_t seq = store->revision();
        // Otherwise, determine the current revision based on timestamp

        ReplicationClient::State current = client.findCurrentState(
            store->revisionTimestamp(), target);
        uint32_t seq = current.revision;
        currentRevision_ = seq;

        if (seq >= target.revision) return;       // No changes

        status_ = Status::PARTIALLY_FETCHED;

        updater_.beginUpdate(current.revision, current.timestamp,
            target.revision, target.timestamp);

        do
        {
            if (stopFetching_) [[unlikely]]
            {
                break;
            }

            seq++;
            try
            {
                LOGS << "Fetching revision " << seq;
                client.fetch(seq, data);
            }
            catch (std::exception& ex)
            {
                LOGS << "ChangeIngester::performDownload ending download due to "
                    << typeid(ex).name() << ": " << ex.what();
                error_ = ex.what();
                break;
            }

            // TODO: This could hang if 0 bytes (because empty data is
            //  used to indicate end of downloads)

            LOGS << "Posting " << data.size() << " bytes of revision data";
            queue_.put(std::move(data));
        }
        while (seq < target.revision);
        if (error_.empty()) [[likely]]
        {
            status_ = Status::FULLY_FETCHED;
        }
    }
    catch (std::exception& ex)
    {
        LOGS << "ChangeIngester::performDownload caught "
            << typeid(ex).name() << ": " << ex.what();
        error_ = ex.what();
    }
    data.clear();
    queue_.put(std::move(data));   // post empty vector to signal end
}


void ChangeIngester::processTask(std::vector<std::byte>& data)
{
    currentRevision_++;
    updater_.setReadingTask(currentRevision_);

    /*
    LOGS << "Inflating " << data.size() << " bytes of gzipped changes";
    ByteBlock osc = Zip::inflateGzip(data);
    LOGS << "  Processing " << osc.size() << " bytes of changes";
    osc.data()[osc.size()-1] = '\0';
    // force null-terminator, possibly overwriting > of final closing tag
    ChangeReader reader(updater_.model(), reinterpret_cast<char*>(osc.data()));
    */

    LOGS << "  Processing " << currentRevision_ << ": "
        << data.size() << " bytes (uncompressed)";
    data[data.size()-1] = static_cast<std::byte>(0);
    // force null-terminator, possibly overwriting > of final closing tag
    ChangeReader reader(updater_.model(), reinterpret_cast<char*>(data.data()));

    //assert(_CrtCheckMemory());
    reader.read();
    // updater_.reportFileRead(osc.size());
    updater_.reportFileRead(data.size());
}