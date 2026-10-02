// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstdint>
#include <memory>



class BulkIdIndexer
{
public:
	BulkIdIndexer();

	void addTile(uint32_t tile);
	void end()
	{
		addTile(END_TILES);
		endLeaf(true);
	}

private:
	static constexpr int MAX_BRANCH_LEVELS = 6;
	static constexpr size_t MAX_RUN_SIZE = 12;
		// tip is max 24 bits, plus sign, plus flag = 26 bits,
		// which means the first varint is max 4 bytes
		// ID space is 52 bits, so run length as varing
		// takes no more than 8 bytes
	static constexpr size_t LEAF_TAIL_SIZE = 4;
	static constexpr uint32_t END_TILES = 0xffff'ffff;
		// checksum

	void endLeaf(bool endIndex);

	uint8_t* p_ = nullptr;
	uint64_t currentStartId_ = 0;
	uint32_t currentTile_ = 0;
	int32_t currentTileDelta_ = 0;
	int64_t currentRunLength_ = 0;
	std::unique_ptr<uint8_t> buffer_;
	uint8_t* pLeafStart_ = nullptr;
	const uint8_t* pSafeLeafEnd_ = nullptr;
	const uint8_t* pLeafBufferEnd_ = nullptr;
	uint32_t branchCounts_[MAX_BRANCH_LEVELS] = {};
	uint32_t pageSizeShift_ = 12;
	uint32_t fanout_ = 340;
};
