// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TReferencedElement.h"
#include <clarisma/data/Deduplicator.h>

// TODO: Fix sorting of elements, don't use anything virtual
// because it bloats the class

// TODO: Would be useful to mark TTagTable to see if it can be quickly compared
// bytewise since it does not contain string pointers
// Idea: Any newly created elements can be compared bytewise, since their pointers
// are the same if they refer to the same handle
// (exisiting elements have different pointers, sicne they are relative to their
// placement)
// Need flags:
// - EXISTING
// - NEEDS_FIXUP


class TSharedElement : public TReferencedElement
{
public:
	TSharedElement(Type type, Handle handle, const uint8_t* data,
		uint32_t size, Alignment alignment, uint32_t hash, int anchor = 0) :
		TReferencedElement(type, handle, data, size, alignment, anchor),
		hash_(hash), users_(0), category_(0)
	{
	}

	uint32_t hash() const { return hash_; }
	void setHash(uint32_t hash) { hash_ = hash; }

	void write(uint8_t* p) const
	{
		memcpy(p, dataStart().ptr(), size());
	}

	// TODO: change
	bool operator<(TSharedElement& other) 
	{
		uint32_t commonSize = std::min(size(), other.size());
		int res = memcmp(dataStart().ptr(), other.dataStart().ptr(), commonSize);
		if (res == 0)
		{
			return size() < other.size();
		}
		return res < 0;
	}

	bool equalsBytewise(const TSharedElement& other) const
	{
		if (size() != other.size()) return false;
		return memcmp(dataStart(), other.dataStart(), size()) == 0;
	}

	int users() const { return users_; }
	void addUser() { users_++; }
	int category() const { return category_; }
	void setCategory(int category) { category_ = category; }

	static constexpr int MIN_COMMON_USAGE = 4;

protected:
	uint32_t hash_;
	unsigned int users_ : 24;
		// TODO: are we ever at risk of exceeding 16M users?
		//  (In theory, if a 320 MB tile only contains 16M objects of same kind)
		//  wrapping this counter could cause it to go to 0, which means
		//  TesWriter won't put it in string table
	unsigned int category_ : 8;
};


template<typename T>
class ElementDeduplicator : public Deduplicator<ElementDeduplicator<T>, T>
{
public:
	static size_t hash(T* item)
	{
		return item->hash();
	}

	static T** next(T* elem)
	{
		return reinterpret_cast<T**>(&elem->next_);
	}
};
