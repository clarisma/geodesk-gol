Make library headers includable only as `geodesk-gol/...` (e.g. `#include "geodesk-gol/build/GolBuilder.h"`), so the unprefixed form `#include "build/GolBuilder.h"` no longer resolves.

## Approach

Namespace the whole library tree under a `geodesk-gol/` directory. `src/` stays the PUBLIC include root (CMakeLists.txt:157 unchanged) — after the move it exposes exactly one top-level entry, `geodesk-gol/`, instead of polluting consumers' global include namespace with `build/`, `tile/`, `gol/`, etc.

## Steps

1. **Move the tree** (preserves git history via rename detection):
   - `mkdir src/geodesk-gol`
   - `git mv src/build src/change src/check src/clarisma src/gol src/osm src/tag src/test.cxx src/tile src/geodesk-gol/`
   - This includes the vendored `src/clarisma/` .cpp implementations and the scratch `src/test.cxx`, so `src/` contains nothing but `geodesk-gol/`.

2. **Rewrite includes** (228 lines in 98 files) with a one-off script over `src/geodesk-gol/**`:
   - For every `#include` line (quoted **and** angle form) whose path starts with `build/`, `change/`, `check/`, `gol/`, `osm/`, `tag/`, or `tile/`, prefix it with `geodesk-gol/` — but only if `src/geodesk-gol/<path>` exists on disk (double safety net: libgeodesk-internal paths such as `geom/Tile.h` or `python/util/util.h` can never be touched).
   - File-relative includes (`"GolTool.h"`, `"util/BoxParser.h"` from `src/gol/`, `"model/ChangeModel.h"` from `src/change/`, platform `"HttpClient_windows.cxx"`) are left alone — the tree moves as a whole, so they keep resolving.

3. **CMakeLists.txt** — two path updates (include dirs and the source GLOB need no change):
   - Line 132: `list(REMOVE_ITEM ... src/gol/main.cpp)` → `src/geodesk-gol/gol/main.cpp`
   - Line 190: `add_executable(gol src/gol/main.cpp)` → `src/geodesk-gol/gol/main.cpp`
   - `file(GLOB_RECURSE ... "src/*.cpp")` still finds all sources recursively; `${CMAKE_CURRENT_SOURCE_DIR}/src` remain the PUBLIC include roots.

4. **Nothing else references the old paths**: `tools/headers.py` walks `src/` recursively (no hardcoded file paths); CI workflows build only the `gol` target; `include/clarisma` vendored headers stay where they are (already namespaced under `clarisma/`).

## Verification

- `grep -rnE '#include [<"](build|tile|change|gol|osm|tag|check)/' src/` returns nothing (old form eliminated).
- Full build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target gol -j$(nproc)` — compiles every library TU plus `main.cpp`, proving all rewritten paths resolve (downloads FetchContent deps: zlib, cpp-httplib, libgeodesk).
- Consumer smoke test: compile a throwaway TU containing `#include "geodesk-gol/build/GolBuilder.h"` with `-Isrc -Iinclude` (flags taken from the generated `compile_commands.json`), then delete it — proves the consumer-facing include form works through the public include dir.

## Alternative rejected

Classic split (public headers into `include/geodesk-gol/`, sources stay in `src/`): produces the same consumer include form but with far more churn — 175 headers move away from their .cpp siblings, every same-name file-relative include breaks, and it forces deciding which headers are "public". Not required for the ask.