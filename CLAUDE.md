# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

renderdoc-mcp is a C++17 MCP (Model Context Protocol) server and CLI that wraps the RenderDoc replay API into 59+ structured tools. It enables AI assistants to open `.rdc` GPU frame captures, inspect pipeline state, debug shaders/pixels, compare captures, and export evidence — all via JSON-RPC over stdio.

The project targets Windows primarily. It links against RenderDoc v1.43 (specified in `renderdoc-version.txt`).

## Build Commands

### Full build (requires RenderDoc source tree):
```bash
cmake -B build -DRENDERDOC_DIR=<path-to-renderdoc-source>
cmake --build build --config Release
```

### Proto-only build (no RenderDoc dependency — builds MCP protocol layer only):
```bash
cmake -B build
cmake --build build --config Release
```

### Build with tests:
```bash
cmake -B build -DRENDERDOC_DIR=<path-to-renderdoc-source> -DBUILD_TESTING=ON
cmake --build build --config Release
```

### Run unit tests (no RenderDoc dependency):
```bash
ctest --test-dir build -L unit -C Release --output-on-failure
```

### Run integration tests (requires RenderDoc):
```bash
ctest --test-dir build -L integration -C Release --output-on-failure
```

### Run a single test binary directly:
```bash
./build/tests/Release/test-unit.exe --gtest_filter="TestSuite.TestName"
```

## Architecture

The codebase has three layers with strict dependency direction: `mcp` → `core` ← `cli`.

### Layer 1: `src/core/` — Business Logic (renderdoc-core library)
- Stateless functions that operate on a `Session` or `DiffSession` object
- Each `.cpp`/`.h` pair covers one domain: `events`, `pipeline`, `shaders`, `pixel`, `debug`, `export`, `capture`, `resources`, `mesh`, `snapshot`, `assertions`, `diff_*`, `pass_analysis`, `counters`, `cbuffer`, etc.
- `Session` wraps RenderDoc's `ICaptureFile` + `IReplayController` — all replay access goes through it
- `DiffSession` manages two parallel replay sessions for capture comparison
- Errors are thrown as `CoreError` with typed error codes (see `errors.h`)
- Types are defined in `types.h` — shared across all layers

### Layer 2: `src/mcp/` — MCP Protocol Layer
- `McpServer` handles JSON-RPC message dispatch (initialize, tools/list, tools/call)
- `ToolRegistry` stores tool definitions (name, JSON schema, handler function) and performs argument validation
- `src/mcp/tools/` — one file per tool group (e.g., `session_tools.cpp`, `pixel_tools.cpp`), each registering tools that call into core functions
- `serialization.cpp` converts core types to/from JSON
- `mcp_server_default.cpp` wires up the default constructor (creates Session + registers all tools)
- The proto library (`renderdoc-mcp-proto`) contains only protocol infrastructure with no RenderDoc dependency

### Layer 3: `src/cli/` — CLI Frontend (renderdoc-cli executable)
- One-shot compound-command CLI that directly calls core functions
- `cli_parse.cpp` handles argument parsing
- Shares core layer with MCP but bypasses the MCP protocol

### Main executable: `src/main.cpp`
- Reads JSON-RPC lines from stdin, dispatches to `McpServer`, writes responses to stdout
- Uses `REPLAY_PROGRAM_MARKER()` to prevent RenderDoc from capturing itself

## Key CMake Variables

| Variable | Required | Description |
|----------|----------|-------------|
| `RENDERDOC_DIR` | For full build | Path to RenderDoc source root (provides API headers) |
| `RENDERDOC_BUILD_DIR` | No | Path to RenderDoc build output if non-standard location |
| `BUILD_TESTING` | No | Set `ON` to build test targets |

## Test Structure

- `tests/unit/` — Tests for MCP protocol layer (no RenderDoc needed): `test_mcp_server`, `test_tool_registry`, `test_serialization`, `test_cli_parse`, `test_pass_analysis`
- `tests/integration/` — Tests requiring RenderDoc: `test_tools*`, `test_protocol`, `test_workflow`, `test_capture`
- Test framework: Google Test v1.14
- Test fixture: `tests/fixtures/vkcube.rdc`

## Conventions

- Namespaces: `renderdoc::core`, `renderdoc::mcp`, `renderdoc::mcp::tools`, `renderdoc::cli`
- Tool registration pattern: each `*_tools.cpp` exports a `registerXxxTools(ToolRegistry&)` function declared in `tools/tools.h`
- Tool handlers return `nlohmann::json` on success or throw `std::runtime_error` for tool-level errors
- Protocol-level errors throw `InvalidParamsError` (converted to JSON-RPC -32602)
- Core errors throw `CoreError` with a typed `Code` enum
- Platform defines: `RENDERDOC_PLATFORM_WIN32`, `RENDERDOC_PLATFORM_LINUX`, `RENDERDOC_PLATFORM_APPLE`

## Adding a New Tool

1. Add core logic in `src/core/<domain>.cpp` with types in `types.h`
2. Create or extend `src/mcp/tools/<domain>_tools.cpp` — register a `ToolDef` with name, description, JSON input schema, and handler
3. Add the register function declaration to `src/mcp/tools/tools.h`
4. Call the register function from `mcp_server_default.cpp`
5. Add serialization for new types in `src/mcp/serialization.cpp`
6. Add source files to `CMakeLists.txt` under the appropriate library target
