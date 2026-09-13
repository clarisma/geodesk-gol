// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "BasicCommand.h"
#include <clarisma/io/File.h>
#include <geodesk/feature/FeatureStore.h>

using namespace geodesk;

class GolCommand : public BasicCommand
{
public:
	GolCommand();
	~GolCommand() override;

	FeatureStore& store() { return store_; }

	int run(char* argv[]) override;

protected:
	static Option COMMON_GOL_OPTIONS[];

	bool setParam(int number, std::string_view value) override;
	void areaOptions(clarisma::CliHelp& help);
	virtual void help() {};
	int setAreaOption(std::string_view value);
	void setArea(const char *value);
	void setAreaFromFile(const char* path);
	void setAreaFromCoords(const char* coords);
	int setBox(std::string_view value);
	// int setCircle(std::string_view value);
	int setOutput(std::string_view value);

	int promptCreate(std::string_view filePath);
	static void checkTilesetGuids(
		const std::string& path1, const clarisma::UUID& guid1,
		const std::string& path2, const clarisma::UUID& guid2);

	static constexpr auto DO_NOT_OPEN = static_cast<FeatureStore::OpenMode>(0xffff'ffff);

	std::string golPath_;
	FeatureStore store_;
	Box bounds_ = Box::ofWorld();
	std::unique_ptr<const Filter> filter_;
	std::string outputFileName_;
	std::string outputTmpFileName_;
	clarisma::File outputFile_;
	FeatureStore::OpenMode openMode_ = FeatureStore::OpenMode::READ;
};
