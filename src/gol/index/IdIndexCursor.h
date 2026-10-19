// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <clarisma/util/varint.h>
#include <clarisma/util/DataPtr.h>



class IdIndexCursor
{
public:
	IdIndexCursor(const uint8_t* data, int pageSizeShift,
		uint32_t rootPage, uint64_t idCount) :
		data_(data), pageSizeShift_(pageSizeShift),
		maxChildCount_(((1u << pageSizeShift) - 16) / 12 + 1)
	{
		path_[0].page = rootPage;
		path_[0].pos = 0;
		path_[0].endId = idCount;
	}

	/// @brief Find the candidate tile for an ID.
	/// @pre IDs must be supplied in nondecreasing order.
	uint32_t seek(uint64_t id)
	{
		path_[0].pos = 0;
		return seekLeaf(&path_[0], 0, id);
	}

	uint32_t seekForward(uint64_t id)
	{
		if (id < leafEndId_)
		{
			while (id >= runEndId_)
			{
				advanceRun();
			}
			return runTile_;
		}
		Branch* pParent = &path_[depth_];
		do
		{
			if (id < pParent->endId)
			{
				pParent->pos++;
				BranchPtr pBranchData(pagePtr(pParent->page));
				assert(pParent->pos <= pBranchData.keyCount());
				return seekLeaf(pParent, (pBranchData
					+ pParent->pos * 8).getUnsignedLong(), id);
			}
			pParent--;
		}
		while (pParent >= path_);
		return 0;
	}


private:
	static constexpr int MAX_BRANCH_LEVELS = 6;

	class BranchPtr : public clarisma::DataPtr
	{
	public:
		BranchPtr(uint8_t* p) : DataPtr(p) {}

		int keyCount() const
		{
			return (*this + 4).getUnsignedShort();
		}

		int level() const
		{
			return (*this + 6).getByte();
		}

		uint64_t* keys() const
		{
			return reinterpret_cast<uint64_t*>(ptr() + 8);
		}

		uint32_t* childPages(int maxChildCount) const
		{
			return reinterpret_cast<uint32_t*>(ptr() + 8 * maxChildCount);
		}

	};

	struct Branch
	{
		uint32_t page;
		uint32_t pos;
		uint64_t endId;
	};

	uint32_t seekLeaf(Branch* pBranch, uint64_t startId, uint64_t id)
	{
		uint64_t childPage;
		uint64_t endId;
		int level;
		for (;;)
		{
			BranchPtr pBranchData(pagePtr(pBranch->page));
			uint32_t keyCount = pBranchData.keyCount();
			level = (pBranchData + 6).getByte();
			assert(keyCount < maxChildCount_ - 1);
			int pos = pBranch->pos;
			assert(pos <= keyCount);
			const uint64_t* pKeys = pBranchData.keys();
			auto iter = std::upper_bound(pKeys + pos, pKeys + keyCount, id);
			pos = iter - pKeys;
			pBranch->pos = pos;
			const uint32_t* pChildren = pBranchData.childPages(maxChildCount_);
			childPage = pChildren[pos];
			startId = pos == 0 ? startId : pKeys[pos-1];
			endId = pos == keyCount ? pBranch->endId : pKeys[pos];
			if (level == 1) break;
			pBranch++;
			pBranch->page = childPage;
			pBranch->pos = 0;
			pBranch->endId = endId;
		}
		depth_ = pBranch - path_;
		assert(depth_ < MAX_BRANCH_LEVELS);
		p_ = pagePtr(childPage) + 4;
		runEndId_ = startId;
		leafEndId_ = endId;
		slot_ = 0;
		tiles_.fill(0);
		do
		{
			advanceRun();
		}
		while (id >= runEndId_);
		return runTile_;
	}

	void advanceRun()
	{
		uint8_t header = *p_++;
		runTile_ = tiles_[header >> 4];
		if (header & 8)
		{
			runTile_ += clarisma::readSignedVarint32(p_);
			tiles_[slot_] = runTile_;
			slot_ = (slot_ + 1) & 0xf;
		}
		uint64_t length = header & 7;
		if (length == 0)
		{
			length = clarisma::readVarint64(p_) + 8;
		}
		runEndId_ += length;
	}

	clarisma::DataPtr pagePtr(uint32_t page) const
	{
		return data_ + (static_cast<uint64_t>(page) << pageSizeShift_);
	}

	const uint8_t* p_ = nullptr;
	// const uint8_t* end_ = nullptr;
	//  TODO: Use this to sanity-check leaves
	uint64_t runEndId_ = 0;
	uint32_t runTile_ = 0;
	uint32_t slot_ = 0;
	uint64_t leafEndId_ = 0;
	const uint8_t* data_ = nullptr;
	const int pageSizeShift_ = 0;
	const int maxChildCount_;
	int depth_ = 0;
	std::array<uint32_t, 16> tiles_;
	Branch path_[MAX_BRANCH_LEVELS];
};
