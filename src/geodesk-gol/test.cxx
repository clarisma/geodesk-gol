// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <windows.h>
#include <io.h>

int main()
{
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode;
    GetConsoleMode(console, &consoleMode);
    consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(console, consoleMode);
    if (!SetConsoleOutputCP(CP_UTF8))
    {
        printf("Failed to enable UTF-8 support.\n");  // TODO
    }

    CONSOLE_CURSOR_INFO cursorInfo;
    if(GetConsoleCursorInfo(console, &cursorInfo))
    {
        cursorInfo.bVisible = false; // Set the cursor visibility
        if(!SetConsoleCursorInfo(console, &cursorInfo))
        {
            printf("Failed to set cursor info.\n");
        }
    }
    else
    {
        printf("Failed to get cursor info.\n");
    }

    int phase = 0;
    while(true)
    {
        /*
        const char *s;
        switch(phase)
        {
        case 0:
            s = " \033[33m • \033[0m\r";
            break;
        case 1:
            s = " \033[33m ● \033[0m\r";
            break;
        case 2:
            s = " \033[33m   \033[0m\r";
            break;
        }
        //strcpy(buf, " \033[33m─────••••• ● ⬤ ●⯈[0m\r");
        // const char* s = arrow ? buf : "        \r";
        DWORD written;
        WriteConsoleA(console, s, static_cast<DWORD>(strlen(s)), &written, NULL);
        phase = (phase + 1) % 3;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        */

        /*
        const char *s;
        switch(phase)
        {
            case 0:
                s = " \033[33m          \033[0m\r";
                break;
            case 1:
                s = " \033[33m ╱        \033[0m\r";
                break;
            case 2:
                s = " \033[33m ╱╲       \033[0m\r";
                break;
            case 3:
                s = " \033[33m ╱╲╱      \033[0m\r";
                break;
            case 4:
                s = " \033[33m  ╲╱╲     \033[0m\r";
                break;
            case 5:
                s = " \033[33m   ╱╲╱    \033[0m\r";
                break;
            case 6:
                s = " \033[33m    ╲╱╲   \033[0m\r";
                break;
            case 7:
                s = " \033[33m     ╱╲╱  \033[0m\r";
                break;
        }
        //strcpy(buf, " \033[33m─────••••• ● ⬤ ●⯈[0m\r");
        // const char* s = arrow ? buf : "        \r";
        DWORD written;
        WriteConsoleA(console, s, static_cast<DWORD>(strlen(s)), &written, NULL);
        phase = (phase + 1) % 8;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        */

        const char *s;
        switch(phase)
        {
            case 0:
                s = " \033[33m   ████   \033[0m\r";
                break;
            case 1:
                s = " \033[33m  ▐████▌  \033[0m\r";
                break;
            case 2:
                s = " \033[33m  ██████  \033[0m\r";
                break;
            case 3:
                s = " \033[33m ▐██████▌  \033[0m\r";
                break;
            case 4:
                s = " \033[33m  ██████  \033[0m\r";
                break;
            case 5:
                s = " \033[33m  ▐████▌  \033[0m\r";
                break;
            case 6:
                s = " \033[33m   ████   \033[0m\r";
                break;
            case 7:
                s = " \033[33m   ▐██▌  \033[0m\r";
            break;
        }
        //strcpy(buf, " \033[33m─────••••• ● ⬤ ●⯈[0m\r");
        // const char* s = arrow ? buf : "        \r";
        DWORD written;
        WriteConsoleA(console, s, static_cast<DWORD>(strlen(s)), &written, NULL);
        phase = (phase + 1) % 8;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
