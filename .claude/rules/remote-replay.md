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

// All tools work — querying the Adreno GPU directly
session_status()
→ { isOpen: true, isRemote: true, remoteHost: "localhost:39920", api: "Vulkan" }

get_pipeline_state(eventId: 100)
list_disassembly_targets()
get_shader(stage: "ps", mode: "disasm")

// When done
disconnect_remote()
```
