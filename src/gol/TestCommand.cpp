// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TestCommand.h"

#include <geodesk/geodesk.h>

#include "clarisma/cli/ConsoleBuffer.h"

using namespace clarisma;
using namespace geodesk;

int TestCommand::run(char* argv[])
{
	int res = BasicCommand::run(argv);
	if (res != 0) return res;

	testContents();
	return 0;
}

bool TestCommand::setParam(int number, std::string_view value)
{
	if (number == 0) return true;   // command itself
	if (number == 1)
	{
		fileName_ = value.data();
		return true;
	}
	if (number == 2)
	{
		testName_ = value.data();
		return true;
	}
	return false;
}


void TestCommand::testContents()
{
	Features world(fileName_);
	uint64_t hash = 0;

	for (auto feature : world)
	{
		hash ^= feature.id();
		for (auto tag : feature.tags())
		{
			std::string_view k = tag.key();
			hash ^= Strings::hash(k.data(), k.size());
			std::string v = tag.value();
			hash ^= Strings::hash(v.data(), v.size());
		}

		Box bounds = feature.bounds();
		hash ^= bounds.minX();
		hash ^= bounds.minY();
		hash ^= bounds.maxX();
		hash ^= bounds.maxY();

		for (auto rel : feature.parents().relations())
		{
			hash ^= rel.id();
		}

		if (feature.isNode())
		{
			hash ^= feature.x();
			hash ^= feature.y();
		}
		else if (feature.isWay())
		{
			for (auto node : feature.nodes())
			{
				hash ^= node.id();
				hash ^= node.x();
				hash ^= node.y();
			}
		}
		else
		{
			assert(feature.isRelation());
			for (auto member : feature.members())
			{
				hash ^= member.id();
				std::string_view role = member.role();
				hash ^= Strings::hash(role.data(), role.size());
			}
		}
	}

	ConsoleBuffer out;
	out << "contents: " << hash;
}

