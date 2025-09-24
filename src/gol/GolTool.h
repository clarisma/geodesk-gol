// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/cli/CliApplication.h>

// TODO ==================================
//  Update License in Headers
//  ======================================
//  ======================================
//  ======================================
//  ======================================
//  ======================================
//  ======================================
//  ======================================


class GolTool : public clarisma::CliApplication
{
public:
	int run(char* argv[]);

private:
	static int build(char* argv[]);
	static int check(char* argv[]);
	static int copy(char* argv[]);
#ifdef GOL_DIAGNOSTICS
	static int dumpTiles(char* argv[]);
	static int test(char* argv[]);
#endif
#ifdef GOL_EXPERIMENTAL
	static int get(char* argv[]);
#endif
	static int info(char* argv[]);
	static int install(char* argv[]);
#ifdef GOL_EXPERIMENTAL
	static int load(char* argv[]);
#endif
	static int map(char* argv[]);
	static int query(char* argv[]);
#ifdef GOL_EXPERIMENTAL
	static int save(char* argv[]);
	static int update(char* argv[]);
#endif
	// void help();
};
