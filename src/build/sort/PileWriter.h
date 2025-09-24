// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <cstring>
#include <memory>
#include <clarisma/alloc/SimpleArena.h>
#include <clarisma/util/protobuf.h>
#include <geodesk/geom/Coordinate.h>
#include "build/util/ParentTileLocator.h"
#include "build/util/ProtoGol.h"

namespace clarisma {
class PileFile;
}

class PileSet
{
public:
	enum GroupType
	{
		LOCAL_NODES = 0,
		LOCAL_WAYS =  8,
	};

	PileSet() :
		pageSize_(16 * 1024),
		arena_(16 * 1024 * 1024),	// must be multiple of 
		// Don't depend on pageSize_ (initialization order is based on 
		// order of fields, and might change)
		firstPile_(nullptr)
	{
	}

	PileSet(PileSet&& other) noexcept :
		arena_(std::move(other.arena_)),
		pageSize_(other.pageSize_),
		firstPile_(other.firstPile_)
	{
		other.firstPile_ = nullptr;
	}

	PileSet& operator=(PileSet&& other) noexcept 
	{
		if (this != &other) 
		{
			arena_ = std::move(other.arena_);
			pageSize_ = other.pageSize_;
			firstPile_ = other.firstPile_;
			other.firstPile_ = nullptr;
		}
		return *this;
	}


	void writeTo(PileFile& file);

	class Page
	{
	protected:
		Page* next_;

		friend class PileSet;
	};

	class Pile : public Page
	{
	public:
		Pile* next() const { return nextPile_; }

		Pile* nextPile_;
		uint32_t number_;
		uint32_t remaining_;
		uint8_t* p_;
		uint64_t prevId_;
		Coordinate prevCoord_;
	};

protected:
	Pile* createPile(uint32_t number, int groupType)
	{
		uint8_t* p = arena_.alloc(pageSize());
		Pile* pile = reinterpret_cast<Pile*>(p);
		pile->next_ = nullptr;
		pile->nextPile_ = firstPile_;
		pile->number_ = number;
		pile->remaining_ = pageSize() - sizeof(Pile) - 1;
		p += sizeof(Pile);
		*p++ = static_cast<uint8_t>(groupType);
		pile->p_ = p;
		pile->prevId_ = 0;
		pile->prevCoord_ = Coordinate(0, 0);
		firstPile_ = pile;
		return pile;
	}

	void addPage(Pile* pile)
	{
		Page* lastPage = reinterpret_cast<Page*>(pile->p_ - pageSize() + pile->remaining_);
		assert(pile->next_ != nullptr || lastPage == pile);
		uint8_t* p = arena_.alloc(pageSize());
		Page* nextPage = reinterpret_cast<Page*>(p);
		nextPage->next_ = nullptr;
		lastPage->next_ = nextPage;
		pile->p_ = p + sizeof(Page);
		pile->remaining_ = pageSize() - sizeof(Page);
	}

	uint32_t pageSize() const { return pageSize_; }

	SimpleArena arena_;
	uint32_t pageSize_;
	Pile* firstPile_;
};


class PileWriter : public PileSet
{
public:
	void write(Pile* pile, ByteSpan data)
	{
		write(pile, data.data(), data.size());
	}

	void write(Pile* pile, const uint8_t* bytes, size_t len)
	{
		for (;;)
		{
			if (len <= pile->remaining_)
			{
				std::memcpy(pile->p_, bytes, len);
				pile->p_ += len;
				pile->remaining_ -= len;
				return;
			}
			std::memcpy(pile->p_, bytes, pile->remaining_);
			// TODO: possible undefined behavior if pile is full
			//  (Invalid pointer, but size = 0)??
			bytes += pile->remaining_;
			len -= pile->remaining_;
			addPage(pile);
		}
	}

	void writeByte(Pile* pile, uint8_t v)
	{
		if (pile->remaining_ == 0) addPage(pile);
		*pile->p_ = v;
		pile->p_++;
		pile->remaining_--;
	}
};

