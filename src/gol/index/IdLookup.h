// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstdint>
#include <memory>

// TODO: The final run doesn't need runLength, because we
//  already know it terminates at the next separator
//  (or max-ID) -- but should we keep it anyway?
//  We'll have to look at the decoding side first


class IdLookup
{
public:

private:
	struct Branch
	{
		const uint8_t* pStart;
		uint32_t pos;
		uint32_t count;
		uint64_t endId;
	};
};
