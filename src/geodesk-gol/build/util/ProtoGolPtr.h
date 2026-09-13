// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "ProtoGol.h"

class ProtoGolPtr
{
public:
	ProtoGolPtr(const uint8_t* p) : p_(p) {}

	std::pair<int, std::string_view> readValueString(const StringCatalog& strings)
	{
		return readString(ProtoStringPair::VALUE, strings);
	}

	std::pair<int, std::string_view> readString(int type, const StringCatalog& strings)
	{
		uint32_t refOrLen = readVarint32(p_);
		if (refOrLen & 1)
		{
			StringCatalog::StringRef ref = strings.stringRef(type, refOrLen >> 1);
			if (ref.isGlobalCode())
			{
				return std::pair(static_cast<int>(ref.globalCode()), std::string_view());
			}
			else
			{
				return std::pair(-1, strings.getString(ref)->toStringView());
			}
		}
		else
		{
			size_t size = refOrLen >> 1;
			std::string_view str(reinterpret_cast<const char*>(p_), size);
			p_ += size;
			return std::pair(-1, str);
		}
	}

private:
	const uint8_t* p_;
};
