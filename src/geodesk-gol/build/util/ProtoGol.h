// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <string_view>
#include <utility>
#include <clarisma/text/Format.h>
#include <clarisma/util/varint.h>
#include <geodesk/feature/types.h>
#include "StringCatalog.h"

using namespace geodesk;

namespace ProtoGol
{
	enum GroupType
	{
		LOCAL_GROUP = 1,
		EXPORTED_GROUP = 2,
		SPECIAL_GROUP = 3,
		EXPORT_TABLE = 4
	};

	enum FeatureType
	{
		NODES = 0,
		WAYS = 1,
		RELATIONS = 2
	};

	enum SpecialNodeFlags
	{
		SHARED = 1,
		ORPHAN = 2
	};

	enum
	{
		LOCAL_NODES        = (FeatureType::NODES << 3) | GroupType::LOCAL_GROUP,
		LOCAL_WAYS         = (FeatureType::WAYS << 3) | GroupType::LOCAL_GROUP,
		LOCAL_RELATIONS    = (FeatureType::RELATIONS << 3) | GroupType::LOCAL_GROUP,
		EXPORTED_NODES     = (FeatureType::NODES << 3) | GroupType::EXPORTED_GROUP,
		EXPORTED_WAYS      = (FeatureType::WAYS << 3) | GroupType::EXPORTED_GROUP,
		EXPORTED_RELATIONS = (FeatureType::RELATIONS << 3) | GroupType::EXPORTED_GROUP,
		COLOCATED_NODES    = (FeatureType::NODES << 3) | GroupType::SPECIAL_GROUP,
	};

	inline void skipString(const uint8_t*& p)
	{
		uint32_t refOrLen = readVarint32(p);
		p += (refOrLen & 1) ? 0 : (refOrLen >> 1);
	}
		
	inline std::pair<int, std::string_view> readString(const uint8_t*& p, int type, const StringCatalog& strings)
	{
		uint32_t refOrLen = readVarint32(p);
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
			std::string_view str(reinterpret_cast<const char*>(p), size);
			p += size;
			return std::pair(-1, str);
		}
	}

	// TODO: Keep in mind that not all global string codes are suitable for keys & roles!

	inline std::pair<int, std::string_view> readKeyString(const uint8_t*& p, const StringCatalog& strings)
	{
		std::pair<int, std::string_view> key = readString(p, ProtoStringPair::KEY, strings);
		assert(key.first <= FeatureConstants::MAX_COMMON_KEY);
		return key;

		// TODO: Make sure StringCatalog enforces this constraint when it builds
		// its lookup table
	}

	inline std::pair<int, std::string_view> readValueString(const uint8_t*& p, const StringCatalog& strings)
	{
		return readString(p, ProtoStringPair::VALUE, strings);
	}

	inline std::pair<int, std::string_view> readRoleString(const uint8_t*& p, const StringCatalog& strings)
	{
		std::pair<int, std::string_view> role = readString(p, ProtoStringPair::VALUE, strings);
		if (role.first > FeatureConstants::MAX_COMMON_ROLE)
		{
			// Not all global string codes can be used as roles, 
			// need to fall back to regular string
			return std::pair<int, std::string_view>(-1,
				strings.getGlobalString(role.first)->toStringView());
		}
		return role;
	}

	inline std::string_view readStringView(const uint8_t*& p, int type, const StringCatalog& strings)
	{
		std::pair<int, std::string_view> str = readString(p, type, strings);
		if (str.first < 0) return str.second;
		return strings.getGlobalString(str.first)->toStringView();
	}

	inline void writeLiteralString(uint8_t*& p, std::string_view s)
	{
		writeVarint(p, s.size() << 1);
		memcpy(p, s.data(), s.size());
		p += s.size();
	}

	inline void writeLiteralInt(uint8_t*& p, int n)
	{
		char buf[32];
		writeLiteralString(p, std::string_view(buf, Format::integer(buf, n) - buf));
	}
}