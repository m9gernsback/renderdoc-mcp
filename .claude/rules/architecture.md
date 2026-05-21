# Project Architecture

## Overview

renderdoc-mcp is a C++17 MCP server and CLI wrapping RenderDoc's replay API into 59+ structured tools. It communicates via JSON-RPC over stdio, enabling AI assistants to inspect GPU frame captures.

Target platform: Windows. Links against RenderDoc v1.43 (see `renderdoc-version.txt`).

## Three-Layer Architecture

Dependency direction: `mcp` -> `core` <- `cli`

### src/core/ — Business Logic (renderdoc-core)
- Stateless functions operating on `Session` or `DiffSession`
- Each .cpp/.h pair covers one domain: events, pipeline, shaders, pixel, debug, export, capture, resources, mesh, snapshot, assertions, diff_*, pass_analysis, counters, cbuffer
- `Session` wraps RenderDoc's `ICaptureFile` + `IReplayController`
- `DiffSession` manages two parallel replay sessions for capture comparison
- Errors thrown as `CoreError` with typed error codes (see `errors.h`)
- All shared types defined in `types.h`

### src/mcp/ — MCP Protocol Layer
- `McpServer` — JSON-RPC dispatch (initialize, tools/list, tools/call)
- `ToolRegistry` — stores tool definitions (name, JSON schema, handler) and validates arguments
- `src/mcp/tools/` — one file per tool group, each registers tools calling into core
- `serialization.cpp` — converts core types to/from JSON
- `renderdoc-mcp-proto` library contains only protocol infra (no RenderDoc dependency)

### src/cli/ — CLI Frontend (renderdoc-cli)
- One-shot compound-command CLI calling core functions directly
- `cli_parse.cpp` handles argument parsing

### src/main.cpp — MCP Server Entry Point
- Reads JSON-RPC lines from stdin, dispatches to McpServer, writes to stdout
- Uses `REPLAY_PROGRAM_MARKER()` to prevent self-capture

## Key CMake Targets

| Target | Type | Description |
|--------|------|-------------|
| `renderdoc-mcp-proto` | Static lib | Protocol layer only (no RenderDoc needed) |
| `renderdoc-core` | Static lib | Core business logic (needs RENDERDOC_DIR) |
| `renderdoc-mcp-lib` | Static lib | Tools wiring core -> MCP |
| `renderdoc-mcp` | Executable | MCP server |
| `renderdoc-cli` | Executable | CLI tool |

## Namespaces

- `renderdoc::core` — business logic and types
- `renderdoc::mcp` — MCP server, tool registry, serialization
- `renderdoc::mcp::tools` — tool registration functions
- `renderdoc::cli` — CLI argument parsing
