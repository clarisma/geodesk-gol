// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "GolCommand.h"
#include <vector>

class UpdateCommand : public GolCommand
{
public:
    UpdateCommand();

    int run(char* argv[]) override;

private:
    static Option UPDATE_OPTIONS[];
    static constexpr size_t MIN_DEFAULT_BUFFER_SIZE = 2ULL * 1024 * 1024 * 1024;

    bool setParam(int number, std::string_view value) override;
    int setBufferSize(std::string_view s);
    void help() override;

    std::string_view url_;
    std::vector<const char*> files_;
    size_t bufferSize_ = 0;
};