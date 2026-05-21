# Shader Tools & External Tool Integration

## Built-in Disassembly Targets

RenderDoc's replay API (`GetDisassemblyTargets()`) only returns built-in targets:
- "SPIR-V (RenderDoc)" — native SPIR-V disassembly
- "KHR_pipeline_executable_properties" — if the Vulkan extension is available
- "AMD GCN ISA" — if AMD shader info extension is present

These are what `list_disassembly_targets` and `get_shader(target: "...")` expose.

## External Shader Tools (AdrenoOfflineCompiler, spirv-dis, etc.)

External tools like AdrenoOfflineCompiler are a **GUI-only feature** in RenderDoc. They live in `qrenderdoc` (the Qt GUI), not in `renderdoc.dll` (the replay API). The GUI reads `ShaderProcessors` from its persistent config, writes raw SPIR-V bytes to a temp file, runs the external executable, and reads its output.

Since the MCP server uses `renderdoc.dll` directly (no GUI), external tools do not appear in `GetDisassemblyTargets()`. Instead, the `run_shader_tool` MCP tool replicates the GUI's approach.

## `run_shader_tool` Implementation

The tool works by:
1. Extracting `ShaderReflection::rawBytes` (the raw shader binary, e.g. SPIR-V)
2. Writing it to a temp file (`%TEMP%/renderdoc-mcp/shader_input.bin`)
3. Expanding command-line placeholders in user-provided args
4. Executing the external tool via `_popen`/`popen`
5. Reading output from stdout or `{output_file}` if specified
6. Returning the result as JSON

### Placeholders

| Placeholder | Replaced with |
|-------------|---------------|
| `{input_file}` | Path to temp file containing shader raw bytes |
| `{output_file}` | Path for tool to write output (tool output read from here if present) |
| `{entry_point}` | Shader entry point from reflection (usually "main") |
| `{stage}` | Long stage name: vert/tesc/tese/geom/frag/comp |

### Key RenderDoc API details

- `ShaderReflection::rawBytes` — `bytebuf` field containing the shader binary
- `ShaderReflection::encoding` — `ShaderEncoding` enum indicating format (SPIRV, DXBC, HLSL, etc.)
- Both accessible via `getShaderStageInfo(ctrl, stage).reflection->rawBytes`
- Available for all APIs: D3D11, D3D12, OpenGL, Vulkan

## Shader Tool Categories

| Tool | Purpose |
|------|---------|
| `list_disassembly_targets` | List built-in disassembly formats from the replay API |
| `get_shader` (mode=disasm, target=...) | Disassemble using a built-in target |
| `get_shader` (mode=reflect) | Get shader reflection (inputs, outputs, constant blocks) |
| `run_shader_tool` | Run any external tool on the raw shader bytes |
| `list_shaders` | List all unique shaders in the capture |
| `search_shaders` | Text search across shader disassembly |
| `shader_build` / `shader_replace` / `shader_restore` | Hot-edit shaders |

## Example usage with external tools

```
// AdrenoOfflineCompiler
run_shader_tool(stage: "ps", executable: "C:\\tools\\AdrenoOfflineCompiler.exe", args: "{input_file} -o {output_file}")

// spirv-dis (SPIR-V to assembly text)
run_shader_tool(stage: "ps", executable: "spirv-dis", args: "{input_file}")

// spirv-cross (SPIR-V to GLSL)
run_shader_tool(stage: "ps", executable: "spirv-cross", args: "{input_file} --output {output_file}")
```
