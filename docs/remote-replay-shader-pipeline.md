# Remote Replay Shader Analysis Pipeline

## Prerequisites

- `renderdoccmd` running on the Android device
- ADB port forward: `adb forward tcp:39920 localabstract:renderdoc_39920`
- RenderDoc GUI must be **closed** (exclusive connection)

## Step-by-Step Pipeline

### Step 1: Open capture with remote replay

```
open_capture(path: "E:\\GitHub\\Outfit_example.rdc", host: "localhost:39920")
```

Expected result: `{ api: "Vulkan", totalDraws: 504, totalEvents: 1743 }`

### Step 2: Find a draw call and navigate to it

```
list_draws()
goto_event(eventId: 1343)
```

**CRITICAL: Always call `goto_event` first before any shader/pipeline queries.** This navigates the replay to that event and caches the state. All subsequent calls with the same `eventId` will reuse the cached state without hitting the device again.

### Step 3: Get pipeline state (optional — for render target info)

```
get_pipeline_state(eventId: 1343)
```

Returns render targets, viewports, depth target. Shader bindings may show `ResourceId::0` — this is normal for remote replay. Use `get_shader` directly for shaders.

### Step 4: Get shader disassembly

```
get_shader(stage: "ps", eventId: 1343, mode: "disasm")
```

**IMPORTANT RULES:**
- Always pass BOTH `stage` AND `eventId` explicitly
- Always use the SAME `eventId` as the `goto_event` call — this avoids redundant remote calls
- Valid stages: `"vs"`, `"ps"`, `"cs"`, `"hs"`, `"ds"`, `"gs"`

Expected result: `{ resourceId: "ResourceId::90906", stage: "ps", disassembly: "SPIR-V 1.0 module..." }`

### Step 5: Run Adreno Offline Compiler (AOC)

```
run_shader_tool(
    stage: "ps",
    eventId: 1343,
    executable: "D:\\NGREnv\\RenderDoc_NGR_64.79\\RenderDoc_NGR_64\\plugins\\adreno\\aoc.exe",
    args: "-fs {input_file} -api=Vulkan -arch=a730 -entry_point {entry_point}"
)
```

**AOC args by shader stage:**

| Stage | AOC flag |
|-------|----------|
| Fragment/Pixel (`"ps"`) | `-fs {input_file}` |
| Vertex (`"vs"`) | `-vs {input_file}` |
| Compute (`"cs"`) | `-cs {input_file}` |
| Tess Control (`"hs"`) | `-tcs {input_file}` |
| Tess Eval (`"ds"`) | `-tes {input_file}` |
| Geometry (`"gs"`) | `-gs {input_file}` |

Always include: `-api=Vulkan -arch=a730 -entry_point {entry_point}`

Expected result: `{ exitCode: 0, encoding: "SPIRV", output: "Adreno Offline Compiler...Total instruction count: 689..." }`

### Step 6: Disconnect when done

```
disconnect_remote()
```

## Critical Rules

1. **Always call `goto_event(eventId)` FIRST** — this is the only call that triggers the remote replay proxy. All subsequent calls with the same `eventId` reuse cached state and do NOT hit the device again.

2. **Use the SAME `eventId` across all calls** — `goto_event`, `get_pipeline_state`, `get_shader`, and `run_shader_tool` must all use the same eventId. Changing eventId triggers another remote proxy call which may destabilize the connection.

3. **To analyze a different event, call `goto_event` with the new eventId first** — then query shader/pipeline at that new eventId.

4. **Do NOT call `get_pipeline_state` to find shader resourceIds** — they may be `ResourceId::0` over remote. Call `get_shader(stage, eventId)` directly instead.

5. **The capture is from Adreno 730** — always use `-arch=a730` with AOC.

6. **AOC input is raw SPIR-V binary** — `run_shader_tool` handles extracting raw bytes from the shader and writing them to a temp file automatically. The `{input_file}` placeholder is replaced with the temp file path.

7. **`list_disassembly_targets` is NOT needed for the AOC workflow** — it returns the device's built-in disassembly targets (e.g. "Disassembly", "AMD GCN ISA") which vary by device. For AOC profiling, skip it and go straight to `run_shader_tool`.

8. **Disassembly target names vary by device** — remote replay on Adreno may return "Disassembly" instead of "SPIR-V (RenderDoc)". This is normal.

## Error Recovery

| Error | Cause | Fix |
|-------|-------|-----|
| "No capture is currently open" | Connection lost | Re-run `open_capture` with `host` parameter |
| "No shader bound at stage" | Wrong eventId or stage | Use `list_draws()` to find valid draw eventIds, try `"ps"` or `"vs"` |
| "Network I/O operation failed" | renderdoccmd died | Restart: `adb shell am start -n org.renderdoc.renderdoccmd.arm64/.Loader`, redo adb forward, re-open capture |
| "Remote side of network connection is busy" | Another client connected | Close RenderDoc GUI or other MCP sessions using this device |
| AOC exitCode != 0 | Wrong args or unsupported shader | Check `errors` field in result; verify `-api=Vulkan -arch=a730` |
| Device crashes mid-session | Called SetFrameEvent to multiple different events | Always `goto_event` first, then query at same eventId. To switch events, accept one proxy call per event navigation. |

## Verified Working Example (Full Persistent Session)

All of the following succeed in a **single persistent MCP session** without device crashes:

```
open_capture(path: "E:\\GitHub\\Outfit_example.rdc", host: "localhost:39920")
→ ✅ { api: "Vulkan", totalDraws: 504, totalEvents: 1743 }

goto_event(eventId: 1343)
→ ✅ { eventId: 1343, numIndices: 20250, flags: "Drawcall|Indexed|Instanced" }

get_pipeline_state(eventId: 1343)
→ ✅ { api: "Vulkan", renderTargets: 5, depth: "SceneDepthZ" }

get_shader(stage: "ps", eventId: 1343, mode: "disasm")
→ ✅ { resourceId: "ResourceId::90906", disassembly: 64280 chars of SPIR-V }

run_shader_tool(stage: "ps", eventId: 1343, executable: "D:\\NGREnv\\RenderDoc_NGR_64.79\\RenderDoc_NGR_64\\plugins\\adreno\\aoc.exe", args: "-fs {input_file} -api=Vulkan -arch=a730 -entry_point {entry_point}")
→ ✅ { exitCode: 0, output: "AOC v5.0...Total instruction count: 1625, ALU 32-bit: 281, ALU 16-bit: 516, Texture reads: 18, Occupancy: 50%" }

disconnect_remote()
→ ✅ { status: "disconnected" }
```
