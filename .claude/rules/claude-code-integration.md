# Claude Code MCP Integration

## Configuring renderdoc-mcp for Claude Code

### Project-level (.mcp.json in project root)

```json
{
  "mcpServers": {
    "renderdoc-mcp": {
      "command": "E:\\GitHub\\renderdoc-mcp\\build\\Release\\renderdoc-mcp.exe",
      "args": []
    }
  }
}
```

### Global (~/.claude/settings.json)

```json
{
  "mcpServers": {
    "renderdoc-mcp": {
      "command": "E:\\GitHub\\renderdoc-mcp\\build\\Release\\renderdoc-mcp.exe",
      "args": []
    }
  }
}
```

## WSL Considerations

The exe runs as a native Windows process even when launched from WSL. This means:

- **Paths in .mcp.json**: Use either the Windows path (`E:\\GitHub\\...`) or the WSL mount path (`/mnt/e/GitHub/.../renderdoc-mcp.exe`) — both work since WSL interop handles the translation.
- **Paths passed to tools**: When calling tools like `open_capture`, use **Windows-style paths** (e.g., `E:\captures\frame.rdc`) because the server process is a native Windows binary that doesn't understand `/mnt/...` paths.

## Protocol Version Compatibility

The MCP server must accept whatever `protocolVersion` the client sends during `initialize`. Claude Code currently sends `2024-11-05`. The server echoes back the client's version to confirm compatibility. Strict version checking will cause the connection to appear "disconnected" in Claude Code.

Key code: `src/mcp/mcp_server.cpp` in `handleInitialize()` — the server accepts the client's version and echoes it back in the response.

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Server shows "disconnected" | Protocol version mismatch — server rejects the client's version | Ensure `handleInitialize` does not reject unknown versions |
| Server shows "disconnected" | Exe not found or crashes on start | Verify exe exists and `renderdoc.dll` is next to it |
| Tools fail with path errors | WSL paths passed to Windows exe | Use Windows-style paths (e.g., `E:\...`) for .rdc files |
