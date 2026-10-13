// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "DownloadCoordinator.h"

HttpClient* DownloadCoordinator::createInitialConnectionUnlocked()
{
    assert(clients_.empty());
    HttpClient* client = createConnection();
    availableConnectionBitset_ = 1;
        // The initial connection is considered available;
        // since we're operating in non-concurrent mode,
        // there is no expectation for the caller to release it
    return client;
}

HttpClient* DownloadCoordinator::createConnection()
{
    clients_.emplace_back(origin_, userAgent_);
    return &clients_.back();
}

HttpClient* DownloadCoordinator::takeConnectionFast()
{
    assert(availableConnectionBitset_ != 0 ||
        clients_.size() < maxConnections_);

    if (availableConnectionBitset_)
    {
        int n = Bits::countTrailingZerosInNonZero(
            availableConnectionBitset_);
        availableConnectionBitset_ &= ~(1ULL << n);
        return &clients_[n];
    }
    return createConnection();
}

HttpClient* DownloadCoordinator::takeConnection()
{
    std::unique_lock lock(mutex_);
    for (;;)
    {
        if (totalTickets_ == 0) return nullptr;
        if (availableConnectionBitset_ ||
            clients_.size() < maxConnections_)
        {
            return takeConnectionFast();
        }
        available_.wait(lock);
    }
}

void DownloadCoordinator::releaseConnection(const HttpClient* client)
{
    {
        std::unique_lock lock(mutex_);
        int n = client - &clients_[0];
        assert (n >= 0 && n < clients_.size());
        assert((availableConnectionBitset_ & (1ULL << n)) == 0);
        availableConnectionBitset_ |= 1ULL << n;
    }
    available_.notify_all();
        // Must notify all, not just one, because one worker
        // may be waiting on a ticket, and the other on a connection,
        // so we want to avoid waking up only the ticket-awaiter
}

std::pair<HttpClient*, uint32_t> DownloadCoordinator::takeConnectionAndTicket()
{
    std::unique_lock lock(mutex_);
    for (;;)
    {
        if (currentTicket_ < totalTickets_ &&
            (availableConnectionBitset_ ||
            clients_.size() < maxConnections_))
        {
            HttpClient* client = takeConnectionFast();
            uint32_t ticket = currentTicket_;
            currentTicket_++;
            return { client, ticket };
        }
        if (totalTickets_ == 0) return {nullptr, 0};
        available_.wait(lock);
    }
}

void DownloadCoordinator::setTotalTickets(uint32_t totalTickets)
{
    {
        std::unique_lock lock(mutex_);
        totalTickets_ = totalTickets;
    }
    available_.notify_all();
}


