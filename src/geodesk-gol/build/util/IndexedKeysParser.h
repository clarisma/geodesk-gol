// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <vector>
#include "tag/AbstractTagsParser.h"
#include "IndexedKey.h"

class IndexedKeysParser : public AbstractTagsParser
{
public:
	explicit IndexedKeysParser(const char* s) :
		AbstractTagsParser(s) {}

	std::vector<IndexedKey> parse();

private:
	static constexpr int MAX_INDEXED_KEYS = 255;
	static constexpr int MAX_INDEX_CATEGORIES = 30;
};
