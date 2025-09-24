// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "GolTool.h"

int main(int argc, char* argv[])
{
#ifndef NDEBUG
	// _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	GolTool app;
	return app.run(argv);
}
