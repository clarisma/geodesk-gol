// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "IndexedKeysParser.h"

#include <unordered_set>


std::vector<IndexedKey> IndexedKeysParser::parse()
{
	std::vector<IndexedKey> indexedKeys;
	std::unordered_set<std::string_view> keys;

	int currentCategory = 0;
	for (;;)
	{
		skipWhitespace();
		if (*pNext_ == 0) break;
		std::string_view key = expectKey();
		if (indexedKeys.size() == MAX_INDEXED_KEYS)
		{
			error("Too many keys (Maximum %d)", MAX_INDEXED_KEYS);	// throws
		}
		currentCategory++;
		if (currentCategory > MAX_INDEX_CATEGORIES)  // category numbers are 1-based
		{
			error("Too many index categories (Maximum %d)", MAX_INDEX_CATEGORIES);	// throws
		}
		auto result = keys.insert(key);
		if(!result.second)
		{
			error("Duplicate key: %.*s", static_cast<int>(key.size()), key.data());  // throws
		}

		indexedKeys.emplace_back(key, currentCategory);
		if (accept('/'))
		{
			if(currentCategory==0) error("Expected key");		// throws
			currentCategory--;
		}
		else
		{
			accept(',');		// optional ,
		}
	}
	return indexedKeys;
}

