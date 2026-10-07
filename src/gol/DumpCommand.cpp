// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "DumpCommand.h"
#include <clarisma/io/File.h>
#include <clarisma/io/FilePath.h>
#include <clarisma/text/Format.h>
#include <clarisma/util/Buffer.h>
#include <geodesk/feature/FeatureStore.h>

#include "clarisma/validate/Validate.h"
#include "tile/util/TileDumper.h"

int DumpCommand::run(char* argv[])
{
	int res = GolCommand::run(argv);
	if (res != 0) return res;

	switch (what_)
	{
	case STRINGS:
		break;
	case FREE:
		dumpFreeRanges();
		break;
	default:
		break;
	}
	return 0;
}

bool DumpCommand::setParam(int number, std::string_view value)
{
	static constexpr const char* THINGS[] = { "strings", "free", nullptr };

	if (GolCommand::setParam(number, value)) return true;
	if (number == 2)
	{
		int what = 0;
		while (THINGS[what])
		{
			if (value == THINGS[what])
			{
				what_ = what;
				return true;
			}
			what++;
		}
		throw ValueException("Unknown type: %s", value.data());
			// safe: value is 0-terminated
	}
	return false;
}


void DumpCommand::dumpFreeRanges()
{
	ConsoleWriter out;
	out << "first,size\n";

	uint32_t freeRangeCount = store().header()->freeRanges;
	if (freeRangeCount)
	{
		DataPtr pFree = store().pagePointer(
			store().header()->freeRangeIndex);
		for (int i = 0; i < freeRangeCount; i++)
		{
			pFree += 8;
			uint64_t range = pFree.getUnsignedLong();
			uint32_t first = static_cast<uint32_t>(range >> 32);
			uint32_t size = static_cast<uint32_t>(range) >> 1;
			out << first << "," << size << "\n";
		}
	}
}
