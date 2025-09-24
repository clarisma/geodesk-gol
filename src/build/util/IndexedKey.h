// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <string_view>

struct IndexedKey
{
	IndexedKey(std::string_view k, int cat) : key(k), category(cat) {}

	const std::string_view key;
	const int category;
};