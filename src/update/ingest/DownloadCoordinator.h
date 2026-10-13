// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <string_view>
#include <thread>
#include <clarisma/data/StaticVector.h>
#include <clarisma/thread/BlockingQueue.h>
#include <clarisma/net/HttpClient.h>

using namespace clarisma;

/// @brief Manages connections and tickets for downloading resources.
/// Maintains up to `maxConnection` active connections to the given
/// origin, and doles out tickets and connections to worker threads
/// that perform the actual downloads.
/// `totalTickets` tracks the number of resources to be downloaded.
/// A worker can request a ticket, along with a connection, using
/// takeConnectionAndTicket() -- if neither is available, this method
/// blocks. Likewise, a worker can request a connection only (to
/// retry a failed download of its current ticket).
/// Setting the total number of tickets to 0 ends all downloads --
/// any workers attempting to request a ticket or connection after
/// the total has been set to 0 receives a nullptr connection,
/// which signals the worker to quit.
///
/// The references to `origin` and `userAgent` are non-owning;
/// callers must ensure these strings outlive the lifetime of
/// this class.
///
class DownloadCoordinator
{
public:
    DownloadCoordinator(
        std::string_view origin,
        std::string_view userAgent,
        int maxConnections) :
        origin_(origin),
        userAgent_(userAgent),
        maxConnections_(maxConnections)
    {
        assert(maxConnections_ > 0 && maxConnections_ <= MAX_CONNECTIONS);
    }

    static constexpr int MAX_CONNECTIONS = 8;

    HttpClient* createInitialConnectionUnlocked();

    /// Requests a ticket and connection, blocking until both
    /// are available. Returns nullptr to signal that downloading
    /// should stop.
    ///
    std::pair<HttpClient*, uint32_t> takeConnectionAndTicket();

    /// Requests a connection, blocking until one is available.
    /// Returns nullptr to signal that downloading should stop.
    ///
    HttpClient* takeConnection();

    /// Releases the given connection back to the pool.
    ///
    void releaseConnection(const HttpClient* client);
    void setTotalTickets(uint32_t totalTickets);
    void endDownloads()
    {
        setTotalTickets(0);
    }

private:
    HttpClient* takeConnectionFast();
    HttpClient* createConnection();

    std::string_view origin_;
    std::string_view userAgent_;
    StaticVector<HttpClient, MAX_CONNECTIONS> clients_;
    uint32_t currentTicket_ = 0;
    uint32_t totalTickets_ = 0;
    uint64_t availableConnectionBitset_ = 0;
        // A bitset that tracks which connections are available
    std::mutex mutex_;
    std::condition_variable available_;
    int maxConnections_ = 0;
};
