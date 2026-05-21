# Development Conventions

## Adding a New Tool

1. Add core logic in `src/core/<domain>.cpp` with types in `core/types.h`
2. Create or extend `src/mcp/tools/<domain>_tools.cpp` — register a `ToolDef` with name, description, JSON input schema, and handler
3. Add the register function declaration to `src/mcp/tools/tools.h`
4. Call the register function from `src/mcp/mcp_server_default.cpp`
5. Add JSON serialization for new types in `src/mcp/serialization.cpp`
6. Add source files to `CMakeLists.txt` under the appropriate library target

## Error Handling

- Core layer: throw `CoreError` with a typed `Code` enum
- MCP layer: throw `InvalidParamsError` for validation failures (becomes JSON-RPC -32602)
- Tool handlers: throw `std::runtime_error` for tool-level errors
- Tool handlers return `nlohmann::json` on success

## Tool Registration Pattern

Each `*_tools.cpp` exports a function declared in `tools/tools.h`:
```cpp
void registerXxxTools(ToolRegistry& registry);
```

Each tool is a `ToolDef` struct:
```cpp
ToolDef{name, description, inputSchema (JSON), handler_function}
```

## Platform Defines

- `RENDERDOC_PLATFORM_WIN32`
- `RENDERDOC_PLATFORM_LINUX`
- `RENDERDOC_PLATFORM_APPLE`

## Testing

- Framework: Google Test v1.14 (fetched via FetchContent)
- Unit tests: `tests/unit/` — no RenderDoc dependency, label `unit`
- Integration tests: `tests/integration/` — require RenderDoc, label `integration`
- Test fixture: `tests/fixtures/vkcube.rdc`
- Run single test: `test-unit.exe --gtest_filter="TestSuite.TestName"`

## CMake Variables

| Variable | Required | Description |
|----------|----------|-------------|
| `RENDERDOC_DIR` | For full build | RenderDoc source root (provides API headers) |
| `RENDERDOC_BUILD_DIR` | No | RenderDoc build output if non-standard location |
| `BUILD_TESTING` | No | Set ON to build test targets |
