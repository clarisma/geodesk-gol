// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <geodesk/feature/types.h>
#include "tile/model/TString.h"

	// Keys must be stripped of all flags, just the code
	// TODO: We could simply add string handles, no need to get
	//  the hashcode of the string

class TagTableHasher
{
public:
	TagTableHasher() noexcept : hash_(5381) {};		// djb2 start value

	size_t hash() const noexcept { return hash_; }

	void addKey(uint32_t k) noexcept
	{
		assert(k <= FeatureConstants::MAX_COMMON_KEY);
		addValue(k);
	}

	void addKey(TString* v) noexcept
	{
		addValue(v);
	}

	void addValue(uint32_t v) noexcept
	{
		hash_ = ((hash_ << 5) + hash_) + v;
		// LOG("  hash = %lld", hash_);
	}

	void addValue(TString* v) noexcept
	{
		hash_ ^= v->hash();
		// LOG("  hash = %lld", hash_);
	}

private:
	size_t hash_;
};
