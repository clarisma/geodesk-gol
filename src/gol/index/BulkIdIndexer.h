// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstdint>
#include <memory>



class BulkIdIndexer
{
public:
	BulkIdIndexer()
	{
		size_t pageSize = 1 << pageSizeShift_;
		size_t pagesInBuffer = fanout_ + MAX_BRANCH_LEVELS;
		buffer_.reset(new uint8_t[pagesInBuffer << pageSizeShift_]);
		uint8_t* p = buffer_.get();
		for (int level=MAX_BRANCH_LEVELS; level>0; --level)
		{
			// TODO: initialize branches
			p += pageSize;
		}
		pLeafStart_ = p;
		pSafeLeafEnd_ = p + pageSize - MAX_RUN_SIZE - LEAF_TAIL_SIZE;
		p_ = pLeafStart_;
		pLeafBufferEnd_ = p + pageSize * fanout_;
	}

	void addTile(uint32_t tile)
	{
		if (tile == currentTile_ || tile == 0)
		{
			currentRunLength_++;
			return;
		}
		int32_t tileDelta = static_cast<int32_t>(tile) - currentTile_;
		uint32_t taggedTileDelta = (clarisma::toZigzag(tileDelta) << 1)
			| (currentRunLength_ > 1 ? 1 : 0);
		if (p_ >= pSafeLeafEnd_)	[[unlikely]]
		{
			// The run might not fit

			uint32_t taggedTileDeltaSize = clarisma::varintSize(
				taggedTileDelta);
			uint32_t runLengthSize = clarisma::varintSize(
				currentRunLength_);
			uint32_t runSize = taggedTileDeltaSize +
				(currentRunLength_ > 1 ? runLengthSize : 0);
			if (p_ + runSize - MAX_RUN_SIZE - LEAF_TAIL_SIZE
				> pSafeLeafEnd_)	// TODO: check
			{
				// It definitively won't fit -> we need to
				// start a new leaf
				endLeaf(false);
			}
		}
		clarisma::writeVarint(p_, taggedTileDelta);
		clarisma::writeVarint(p_, currentRunLength_);
		currentTile_ = tile;
		currentRunLength_ = 1;
	}

	void endLeaf(bool endIndex)
	{
		bool flush = endIndex;

		// TODO: write checksum to tail
		*reinterpret_cast<uint32_t*>(pLeafStart_) =
			clarisma::Pointers::offset32(p_, pLeafStart_) - 4;
		pLeafStart_ += 1 << pageSizeShift_;

		int branchLevel = 0;
		uint8_t* pBranch = buffer_.get() +
			((MAX_BRANCH_LEVELS-1) << pageSizeShift_);
		for (;;)
		{
			uint32_t branchCount = branchCounts_[branchLevel];
			if (branchCount < fanout_)
			{
				// We can still add an entry to the branch
				branchCount++;
				branchCounts_[branchLevel] = branchCount;
				*reinterpret_cast<uint64_t*>(pBranch +
					sizeof(uint64_t) * branchCount) = currentStartId_;
				break;
			}
			branchLevel++;
			assert(branchLevel < MAX_BRANCH_LEVELS);
			flush = true;
		}

		flush |= pLeafStart_ == pLeafBufferEnd_;
		if (flush)
		{
			// We need to flush to the store

			uint8_t* pLeafBufferStart = buffer_.get() +
				(MAX_BRANCH_LEVELS << pageSizeShift_);
			size_t leafDataLen = pLeafStart_ - pLeafBufferStart;
			uint32_t leafPages = static_cast<uint32_t>(leafDataLen)
				>> pageSizeShift_;

			if (endIndex)
			{
				for (branchLevel=0; branchLevel<MAX_BRANCH_LEVELS; branchLevel++)
				{
					if (branchCounts_[branchLevel] == 0) break;
				}
				branchLevel++;
			}
			uint32_t totalPages = leafPages + branchLevel;

			// TODO: alloc totalPages, then fill in the page pointers
			// TODO: write buffer into the store

			for (int i=0; i<branchLevel; i++)
			{
				branchCounts_[i] = 0;
			}
			pLeafStart_ = pLeafBufferStart;
		}
		pSafeLeafEnd_ = pLeafStart_ + (1 << pageSizeShift_)
			- MAX_RUN_SIZE - LEAF_TAIL_SIZE;
	}

private:
	static constexpr int MAX_BRANCH_LEVELS = 6;
	static constexpr size_t MAX_RUN_SIZE = 12;
		// tip is max 24 bits, plus sign, plus flag = 26 bits,
		// which means the first varing is max 4 bytes
		// ID space is 52 bits, so run length as varing
		// takes no more than 8 bytes
	static constexpr size_t LEAF_TAIL_SIZE = 4;
		// checksum

	uint8_t* p_ = nullptr;
	uint64_t currentStartId_ = 0;
	uint32_t currentTile_ = 0;
	uint64_t currentRunLength_ = 0;
	std::unique_ptr<uint8_t> buffer_;
	uint8_t* pLeafStart_ = nullptr;
	const uint8_t* pSafeLeafEnd_ = nullptr;
	const uint8_t* pLeafBufferEnd_ = nullptr;
	uint32_t branchCounts_[MAX_BRANCH_LEVELS] = {};
	uint32_t pageSizeShift_ = 12;
	uint32_t fanout_ = 340;
};
