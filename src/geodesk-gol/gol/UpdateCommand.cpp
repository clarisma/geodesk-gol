// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "UpdateCommand.h"
#include <clarisma/cli/CliHelp.h>
#include <clarisma/net/UrlUtils.h>
#include <clarisma/sys/SystemInfo.h>
#include <clarisma/util/FileSize.h>
#include <clarisma/validate/FileSizeParser.h>
#include <clarisma/validate/Validate.h>

#include "change/Updater.h"

UpdateCommand::Option UpdateCommand::UPDATE_OPTIONS[] =
{
    { "buffer",				OPTION_METHOD(&UpdateCommand::setBufferSize) },
    { "B",	    			OPTION_METHOD(&UpdateCommand::setBufferSize) }
};

UpdateCommand::UpdateCommand()
{
    addOptions(UPDATE_OPTIONS, sizeof(UPDATE_OPTIONS) / sizeof(Option));
    openMode_ = FeatureStore::OpenMode::WRITE | FeatureStore::OpenMode::EXCLUSIVE;
        // TODO: concurrent mode
}

bool UpdateCommand::setParam(int number, std::string_view value)
{
    if (GolCommand::setParam(number, value)) return true;
    if (number == 2)
    {
        if (UrlUtils::isUrl(value.data()))  // safe, is 0-terminated
        {
            url_ = value;
            return true;
        }
    }

    if (!url_.empty()) return false;
        // If URL provided, no other params may follow

    if (UrlUtils::isUrl(value.data()))
    {
        throw ValueException("Must be a local file");
    }
    files_.push_back(value.data());
        // This is safe, as value is guaranteed to be 0-terminated
    return true;
}

int UpdateCommand::setBufferSize(std::string_view s)
{
    FileSizeParser parser(s.data());    // safe (0-terminated)
    bufferSize_ = parser.parse();
    return 1;
}

int UpdateCommand::run(char* argv[])
{
    int res = GolCommand::run(argv);
    if (res != 0) return res;

    std::vector<const char*> files;
    /*
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4254.osc");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4255.osc");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4256.osc");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4257.osc");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4258.osc");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4259.osc");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4260.osc");
    */
    // files.push_back("e:\\geodesk\\mapdata\\updates\\de-4255.osc.gz");

    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4255.osc.gz");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4256.osc.gz");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4257.osc.gz");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4258.osc.gz");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4259.osc.gz");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4260.osc.gz");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4261.osc.gz");

    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4262.osc.gz");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4263.osc.gz");
    files.push_back("e:\\geodesk\\mapdata\\updates\\de-4264.osc.gz");

    // assert(_CrtCheckMemory());

    UpdateSettings settings;
    settings.setThreadCount(threadCount());
    if (bufferSize_ == 0)   [[likely]]
    {
        bufferSize_ = std::max(SystemInfo::maxMemory() * 3 / 4,
            MIN_DEFAULT_BUFFER_SIZE);
        // TODO: maxMemory() not impleemnted for MacOS, returns 0
    }
    settings.setBufferSize(bufferSize_);
    LOGS << "Buffer size = " << FileSize(bufferSize_);
    LOGS << "Max memory = " << SystemInfo::maxMemory();
    settings.complete();
    // assert(_CrtCheckMemory());

    Updater updater(&store_, settings);
    // ByteBlock osc = File::readAll("c:\\geodesk\\research\\planet-daily-4443.osc");
    // ByteBlock osc = File::readAll("c:\\geodesk\\research\\planet-hourly-106663.osc");
    updater.update(url_, files_);
    // updater.update("c:\\geodesk\\research\\planet-daily-4443.osc");

    return 0;
}


void UpdateCommand::help()
{
    CliHelp help;
    help.command("gol update <gol-file> [<url> | <file>+] [<options>]",
        "Apply changes from a replication server or local files.");
    areaOptions(help);
    generalOptions(help);
}
