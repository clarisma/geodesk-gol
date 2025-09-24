// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "InstallCommand.h"
#include <clarisma/cli/ConsoleWriter.h>
#include <clarisma/io/File.h>
// #include <io.h>
#include <iostream>
#include <random>

using namespace clarisma;

// Function executed by each thread
void printNumbers(char threadPrefix)
{
    for (int i = 1; i <= 10000; ++i)
    {
        ConsoleWriter out; // (Console::Stream::STDERR); // out;
        out.timestamp() << threadPrefix << i << "\n";
    }
}


int InstallCommand::run(char* argv[])
{
    const int numThreads = 20;
    std::vector<std::thread> threads;

    /*
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    std::cerr << "stdout is redirected to " << File::path(hStdout) << "\n";
    */

    Console::get()->start("Testing...");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));


    // Launch 20 threads
    for (int i = 0; i < numThreads; ++i)
    {
        char threadPrefix = 'A' + i; // Generate prefix 'A', 'B', ..., 'T'
        threads.emplace_back(printNumbers, threadPrefix);
    }

    for(int i = 1; i<=100; i++)
    {
        std::random_device rd;
        std::mt19937 gen(rd()); // Mersenne Twister RNG
        std::uniform_int_distribution<> dist(0, 250); // Range: 0 to 1000 ms
        int sleepDuration = dist(gen); // Random value in milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepDuration));
        Console::get()->setProgress(i);
    }

    // Join all threads
    for (auto& thread : threads)
    {
        thread.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    Console::end();

    return 0;
}
