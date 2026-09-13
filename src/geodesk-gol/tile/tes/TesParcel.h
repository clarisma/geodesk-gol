// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <memory>
#include <clarisma/alloc/Block.h>
#include <clarisma/util/Parcel.h>

class TesParcel;

using TesParcelPtr = clarisma::ParcelPtr<TesParcel>;

// TODO: Need to track start version?
// If first parcel updates from Rev #0, there is no need to load the
// exsting tile, since we're rebuilding it from scratch

class TesParcel : public clarisma::Parcel<TesParcel>
{
public:
	static TesParcelPtr create(uint32_t size, uint32_t sizeUncompressed, uint32_t checksum)
	{
		return Parcel::create(size, sizeUncompressed, checksum);
	}

	uint32_t sizeUncompressed() const { return sizeUncompressed_; }
	void setNext(TesParcelPtr&& p) { next_ = std::move(p); }
	TesParcelPtr takeNext() { return std::move(next_); }
	clarisma::ByteBlock uncompress();

private:
	TesParcel(uint32_t sizeUncompressed, uint32_t checksum) :
		sizeUncompressed_(sizeUncompressed),
		checksum_(checksum)
	{
	}

	TesParcelPtr next_;
	uint32_t sizeUncompressed_;
	uint32_t checksum_;

	friend class Parcel;
};
