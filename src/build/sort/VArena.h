// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <cstdint>
#include "VFeature.h"

// TODO: Section starts must be valid!!!!
// (previous section could end at or just before a page end, which messes
// up the iterators)

class VArena
{
public:
	struct Page
	{
		Page* next;
		Page* prev;
		size_t size;

		static constexpr size_t HEADER_SIZE = 32;

		uint8_t* headerStart()
		{
			return reinterpret_cast<uint8_t*>(this);
		}

		template <typename T>
		T* start()
		{
			return reinterpret_cast<T*>(headerStart() + HEADER_SIZE);
		}

		template <typename T>
		T* end() 
		{
			return reinterpret_cast<T*>(headerStart() + size);
		}

		void free()
		{
			delete[] headerStart();	// TODO
		}
	};

	struct Section
	{
		Section() : page(nullptr), start(nullptr) {}
		Section(Page* page_, void* start_) : 
			page(page_), start(start_) 
		{
			assert(start_ > page_);
		}
		// Section(const Section& other) :	page(other.page), start(other.start) {}

		Page* page;
		void* start;
	};

	template <typename T>
	class Iterator
	{
	public:
		Iterator(Section start, Section end) :
			currentPage_(start.page),
			p_(reinterpret_cast<T*>(start.start)),
			lastPage_(end.page),
			end_(reinterpret_cast<T*>(end.start))
		{
			setCurrentEnd();
		}

		bool hasNext() { return p_ < currentEnd_; }

		T* next()
		{
			assert(hasNext());
			T* next = p_;
			p_++;
			if (p_ >= currentEnd_)
			{
				if (currentPage_ != lastPage_)
				{
					currentPage_ = currentPage_->next;
					assert(currentPage_);
					p_ = currentPage_->start<T>();
					setCurrentEnd();
				}
			}
			return next;
		}

	private:
		void setCurrentEnd()
		{
			currentEnd_ = (currentPage_ == lastPage_) ? end_ : currentPage_->end<T>();
		}

		T* p_;
		Page* currentPage_;
		T* currentEnd_;
		Page* lastPage_;
		T* end_;
	};

	// TODO: Sections are broken!!!!!

	template <typename T>
	class ReverseIterator
	{
	public:
		ReverseIterator(Section start, Section end) :
			currentPage_(end.page),
			p_(reinterpret_cast<T*>(end.start)),
			firstPage_(start.page),
			start_(reinterpret_cast<T*>(start.start))
		{
			setCurrentStart();
			next();
			// A section's end is always the start of the next section
			// If the next section start at the beginning of a page,
		}

		bool hasNext() { return p_ >= currentStart_; }

		T* next()
		{
			assert(hasNext());
			T* next = p_;
			p_--;
			if (p_ < currentStart_)
			{
				if (currentPage_ != firstPage_)
				{
					currentPage_ = currentPage_->prev;
					assert(currentPage_);
					p_ = currentPage_->end<T>() - 1;
					setCurrentStart();
				}
			}
			return next;
		}

		void setCurrentStart()
		{
			currentStart_ = (currentPage_ == firstPage_) ? start_ : currentPage_->start<T>();
		}

	private:
		T* p_;
		Page* currentPage_;
		T* currentStart_;
		Page* firstPage_;
		T* start_;
	};

	Section section()  
	{ 
		// TODO: This assumes that sectioned elements fit onto a page
		// without leaving a gap at the end of the page
		if (p_ == end_) allocPage();
		return { lastPage_, p_ }; 
	}

	VArena(size_t pageSize) :
		pageSize_(pageSize)
	{
		assert(pageSize % sizeof(VLocalNode) == 0);
		assert(pageSize % sizeof(VLocalFeature2D) == 0);
		lastPage_ = allocRawPage(pageSize_);
		resetLastPage();
	}

	~VArena()
	{
		Page* p = lastPage_;
		do
		{
			Page* next = p->next;
			p->free();
			p = next;
		}
		while (p != lastPage_);
	}

	void clear()
	{
		Page* p = lastPage_->next;
		while (p != lastPage_)
		{
			Page* next = p->next;
			p->free();
			p = next;
		}
		resetLastPage();
	}

	template <typename T>
	T* alloc()
	{
		size_t size = sizeof(T);
		if (p_ + size > end_) allocPage();
		T* pObj = reinterpret_cast<T*>(p_);
		p_ += size;
		return pObj;
	}

	template <typename T, typename... Args>
	T* create(Args&&... args)
	{
		T* p = alloc<T>();
		new(p)T(std::forward<Args>(args)...);
		return p;
	}

private:
	void allocPage()
	{
		Page* newPage = allocRawPage(pageSize_);
		newPage->next = lastPage_->next;
		newPage->next->prev = newPage;
		lastPage_->next = newPage;
		newPage->prev = lastPage_;
		lastPage_->size = p_ - lastPage_->headerStart();
		lastPage_ = newPage;
		p_ = newPage->start<uint8_t>();
		end_ = newPage->headerStart() + pageSize_;
	}

	static Page* allocRawPage(size_t size)
	{
		Page* page = reinterpret_cast<Page*>(new uint8_t[size]);
		page->size = size;
		return page;
	}

	void resetLastPage()
	{
		lastPage_->next = lastPage_;
		lastPage_->prev = lastPage_;
		p_ = lastPage_->start<uint8_t>();
		end_ = lastPage_->headerStart() + pageSize_;
	}

	Page* lastPage_;
	uint8_t* p_;
	const uint8_t* end_;
	size_t pageSize_;
};
