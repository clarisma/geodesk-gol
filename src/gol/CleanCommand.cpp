// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "CleanCommand.h"
#include <clarisma/cli/CliHelp.h>
#include <clarisma/cli/Console.h>
#include <clarisma/validate/Validate.h>

#include "update/Updater.h"

/*
CleanCommand::Option UpdateCommand::CLEAN_OPTIONS[] =
{
    { "buffer",				OPTION_METHOD(&UpdateCommand::setBufferSize) },
    { "B",	    			OPTION_METHOD(&UpdateCommand::setBufferSize) },
    { "revision",				OPTION_METHOD(&UpdateCommand::setRevision) },
    { "r",	    			OPTION_METHOD(&UpdateCommand::setRevision) }
};
*/

CleanCommand::CleanCommand()
{
    // addOptions(CLEAN_OPTIONS, sizeof(CLEAN_OPTIONS) / sizeof(Option));
    openMode_ = FeatureStore::OpenMode::WRITE | FeatureStore::OpenMode::EXCLUSIVE;
        // TODO: concurrent mode
}


bool CleanCommand::setParam(int number, std::string_view value)
{
    if (GolCommand::setParam(number, value)) return true;
    return true;
}


int CleanCommand::run(char* argv[])
{
    int res = GolCommand::run(argv);
    if (res != 0) return res;

    uint32_t freeRangeCount = store().header()->freeRanges;
    if (freeRangeCount)
    {
        Console::get()->start("Cleaning...");
        double workPerRange = 100.0 / freeRangeCount;
        double workCompleted = 0;
        DataPtr pFree = store().pagePointer(
            store().header()->freeRangeIndex);
        for (int i = 0; i < freeRangeCount; i++)
        {
            pFree += 8;
            uint64_t range = pFree.getUnsignedLong();
            uint32_t first = static_cast<uint32_t>(range >> 32);
            uint32_t size = static_cast<uint32_t>(range) >> 1;
            store().deallocate(first, size);
            workCompleted += workPerRange;
            Console::get()->setProgress(static_cast<int>(workCompleted));
        }
        Console::end().success() << "Cleaned " << FormattedLong(freeRangeCount) <<
            (freeRangeCount==1 ? " range.\n" : " ranges.\n");
    }

    return 0;
}


void CleanCommand::help()
{
    CliHelp help;
    help.command("gol clean <gol-file>",
        "Free up unused space inside a GOL.");
//    help.option("-r, --revision <n> | latest",
//        "Target revision (default: latest)");
    generalOptions(help);
}
