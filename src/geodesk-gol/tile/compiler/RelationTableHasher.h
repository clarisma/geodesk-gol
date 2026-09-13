// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <geodesk/feature/Tex.h>
#include <geodesk/feature/Tip.h>

class RelationTableHasher
{
public:
	RelationTableHasher() noexcept : hash_(5381) {};		// djb2 start value

	size_t hash() const noexcept { return hash_; }

	void addLocalRelation(int_fast32_t handle) noexcept
	{
		addValue(handle);
	}

	void addTexDelta(TexDelta texDelta)
	{
		addValue(static_cast<uint64_t>(texDelta));
	}

	void addTipDelta(TipDelta tipDelta)
	{
		addValue(static_cast<uint64_t>(tipDelta));
	}

private:
	void addValue(uint64_t v) noexcept
	{
		hash_ = ((hash_ << 5) + hash_) + v;
	}

	size_t hash_;
};