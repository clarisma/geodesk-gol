// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TSharedElement.h"
#include <clarisma/util/Strings.h>

// TODO: TString must enforce the 4-byte size minimum 

class TString : public TSharedElement
{
public:
	TString(Handle handle, const uint8_t* data, uint32_t size, uint32_t hash) :
		TSharedElement(Type::STRING, handle, data, size, Alignment::BYTE, hash)
	{
	}

	/*
	TString(Handle handle, const uint8_t* data) :
		TString(handle, data, getStringSize(data))
	{
	}
	*/

	const ShortVarString* string() const
	{
		return reinterpret_cast<const ShortVarString*>(data().ptr());
	}

	/*
	bool operator<(const TSharedElement& other) const override
	{
		uint32_t ofs1 = 1 + (*data() >> 7);
		uint32_t ofs2 = 1 + (*other.data() >> 7);
		uint32_t len1 = size() - ofs1;
		uint32_t len2 = other.size() - ofs2;
		uint32_t commonLen = std::min(len1, len2);
		int res = memcmp(data() + ofs1, other.data() + ofs2, commonLen);
		if (res == 0)
		{
			return len1 < len2;
		}
		return res < 0;
	}
	*/

	bool operator==(const TString& other) const
	{
		if (hash() != other.hash()) return false;
		// TODO: could even skip the hash check
		return equalsBytewise(other);
	}

	static bool compare(const TString* a, const TString* b)
	{
		return ShortVarString::compare(a->string(), b->string());
	}

	static bool compareGeneric(const TSharedElement* a, const TSharedElement* b)
	{
		return ShortVarString::compare(
			a->cast<TString>()->string(), b->cast<TString>()->string());
	}

	static uint32_t getStringSize(const uint8_t* data)
	{
		uint32_t len = *data;
		return reinterpret_cast<const ShortVarString*>(data)->totalSize();
	}

	static constexpr TElement::Type TYPE = TElement::Type::STRING;
};


class StringDeduplicator : public ElementDeduplicator<TString>
{
public:
	// Hash is 32-bit to match the hash size stored in TSharedElement
	TString* lookup(std::string_view s, uint32_t hash) const
	{
		size_t slot = hash % this->tableSize_;
		TString* existing = this->table_[slot];
		while (existing)
		{
			if (*existing->string() == s) return existing;
			existing = *next(existing);
		}
		return nullptr;
	}
};
