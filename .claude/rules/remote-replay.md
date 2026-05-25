# Remote Replay (Android / Remote Devices)

## Overview

renderdoc-mcp supports remote replay, allowing captures made on Android (or other remote devices) to be replayed on the original hardware. This is essential for captures that use device-specific Vulkan extensions not available on the local PC GPU.

## How It Works

1. `renderdoccmd` (RenderDoc's replay server) runs on the Android device
2. ADB port forwarding maps the device's abstract socket to a local TCP port
3. `open_capture(path, host)` connects via `RENDERDOC_CreateRemoteServerConnection`, copies the capture to the device, and opens replay remotely
4. The returned `IReplayController*` is the same interface as local replay — all tools work transparently

## Setup Steps

### 1. Start renderdoccmd on device

The easiest way is to use the **RenderDoc GUI** (Settings → Android → start replay server). Alternatively:
```
adb shell am start -n org.renderdoc.renderdoccmd.arm64/.Loader
```

### 2. Set up ADB port forwarding

The RenderDoc GUI does this automatically. If manual:
```
adb forward tcp:39920 localabstract:renderdoc_39920
```

### 3. Close the RenderDoc GUI

**Critical**: RenderDoc's remote server only accepts **one connection at a time**. The GUI must be closed/disconnected before the MCP server can connect.

### 4. Open capture via MCP

```
open_capture(path: "E:\\captures\\frame.rdc", host: "localhost:39920")
```

## Important Constraints

### Single Connection

RenderDoc's remote server allows only one active connection. If the RenderDoc GUI is connected, MCP will get "Remote side of network connection is busy".

### DLL Version Match

The `renderdoc.dll` used by renderdoc-mcp **must match** the `renderdoccmd` version on the device. If they differ, you get "Network I/O operation failed" during handshake.

- Installed RenderDoc: `C:\Program Files\RenderDoc\renderdoc.dll`
- Check version: `renderdoccmd.exe --version`
- If mismatch: copy the installed `renderdoc.dll` to `build\Release\`

### ADB Port Conflicts

If something else (like an old adb forward) is occupying port 39920, remove all forwards first:
```
adb forward --remove-all
adb forward tcp:39920 localabstract:renderdoc_39920
```

## Remote Replay Capabilities

| Feature | Works | Notes |
|---------|-------|-------|
| Open capture / metadata | ✅ | API, event count, draw count |
| Event enumeration | ✅ | list_events, list_draws |
| goto_event | ✅ | Navigation works |
| list_disassembly_targets | ✅ | Returns device targets |
| get_pipeline_state — render targets | ✅ | Names, formats, dimensions |
| get_pipeline_state — viewports | ✅ | Viewport dimensions |
| get_pipeline_state — depth target | ✅ | Name, format, dimensions |
| get_pipeline_state — shader bindings | ⚠️ | resourceIds may be 0; use get_shader directly instead |
| get_shader (disasm/reflect) | ✅ | Full SPIR-V disassembly and reflection |
| run_shader_tool | ✅ | External tools work on raw shader bytes |
| export_render_target | ✅ | Works via replay |
| Pixel operations | ✅ | pick_pixel, pixel_history |

**Note**: `get_pipeline_state` may show shader resourceIds as `ResourceId::0` over remote replay, but `get_shader(stage, eventId)` still retrieves the correct shader. Always use `get_shader` directly for shader access during remote replay rather than relying on pipeline state shader bindings.

**Note**: `list_disassembly_targets` returns device-specific target names (e.g. "Disassembly", "AMD GCN ISA") which differ from local replay names (e.g. "SPIR-V (RenderDoc)"). This is normal. For external tool profiling (AOC, spirv-dis, etc.), use `run_shader_tool` directly — it does not depend on disassembly targets.

## MCP Tools for Remote Replay

| Tool | Usage |
|------|-------|
| `open_capture` | Add `host: "localhost:39920"` for remote replay |
| `disconnect_remote` | Close the remote connection and free device resources |
| `session_status` | Shows `isRemote: true` and `remoteHost` when connected remotely |

## Troubleshooting

| Error | Cause | Fix |
|-------|-------|-----|
| "Network I/O operation failed" | renderdoccmd not running, or DLL version mismatch | Start renderdoccmd on device; ensure DLL versions match |
| "Remote side of network connection is busy" | RenderDoc GUI is still connected | Close the RenderDoc GUI first |
| "hardware unsupported or incompatible" with local GPU name | Connected to local `renderdoccmd`, not device | Ensure adb forward points to `localabstract:renderdoc_39920`, not `tcp:39920` |
| renderdoccmd dies on device | App may need to be kept in foreground on some devices | Keep device screen on or use `adb shell am start` to restart |

## Example Session

```
// Open capture for remote replay on Android
open_capture(path: "E:\\captures\\frame.rdc", host: "localhost:39920")
→ { api: "Vulkan", totalEvents: 1743, totalDraws: 504 }

// CRITICAL: Always goto_event FIRST — this is the one remote proxy call
goto_event(eventId: 1343)
→ { eventId: 1343, numIndices: 20250 }

// All subsequent calls at the SAME eventId reuse cached state (no device hit)
get_pipeline_state(eventId: 1343)
get_shader(stage: "ps", eventId: 1343, mode: "disasm")
run_shader_tool(stage: "ps", eventId: 1343, executable: "path/to/aoc.exe", args: "-fs {input_file} -api=Vulkan -arch=a730 -entry_point {entry_point}")

// When done
disconnect_remote()
```

### Key Rules for Remote Replay

1. **Always call `goto_event(eventId)` FIRST** — this triggers the one remote proxy call per event. All subsequent queries at the same eventId reuse cached state without hitting the device.
2. **Use the SAME `eventId` across `goto_event`, `get_pipeline_state`, `get_shader`, `run_shader_tool`** — changing eventId triggers another proxy call. Minimize event changes to avoid device instability.
3. **Do NOT use `get_pipeline_state` to find shader resourceIds** — they may be `ResourceId::0` over remote. Call `get_shader(stage, eventId)` directly instead.
4. **`list_disassembly_targets` is not needed for `run_shader_tool`** — the AOC/external tool workflow extracts raw SPIR-V bytes directly, independent of disassembly targets.
5. **Disassembly target names vary by device** — remote Adreno may return "Disassembly" instead of "SPIR-V (RenderDoc)". This is normal and does not affect functionality.
