# Building for Windows (from Windows terminal)

## Prerequisites

- Visual Studio 2022 (Professional/Enterprise/Community) with C++ desktop workload
- CMake (installed at `C:\Program Files\CMake\bin\cmake.exe`)
- RenderDoc v1.43 source tree at `E:\GitHub\renderdoc`

## Terminal Setup

Open **"x64 Native Tools Command Prompt for VS 2022"** from Start menu.

Or from any terminal:
```bat
"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
```

## Step 1: Build RenderDoc (one-time)

```bat
cd /d E:\GitHub\renderdoc

msbuild renderdoc\3rdparty\breakpad\client\windows\common.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:SolutionDir=E:\GitHub\renderdoc\ /m

msbuild renderdoc\3rdparty\breakpad\client\windows\crash_generation\crash_generation_client.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:SolutionDir=E:\GitHub\renderdoc\ /m

msbuild renderdoc\3rdparty\breakpad\client\windows\handler\exception_handler.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:SolutionDir=E:\GitHub\renderdoc\ /m

msbuild renderdoc\renderdoc.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:SolutionDir=E:\GitHub\renderdoc\ /m
```

Produces: `E:\GitHub\renderdoc\x64\Release\renderdoc.dll` and `renderdoc.lib`

## Step 2: Build renderdoc-mcp

```bat
cd /d E:\GitHub\renderdoc-mcp

cmake -B build -G "Visual Studio 17 2022" -A x64 -DRENDERDOC_DIR=E:\GitHub\renderdoc -DBUILD_TESTING=ON

cmake --build build --config Release
```

## Step 3: Run unit tests

```bat
ctest --test-dir build -L unit -C Release --output-on-failure
```

## Output binaries

```
build\Release\renderdoc-mcp.exe   — MCP server (stdio JSON-RPC)
build\Release\renderdoc-cli.exe   — CLI for shell & CI
```

The post-build step copies `renderdoc.dll` next to the executables automatically.

## Building from WSL

WSL can invoke Windows-side CMake directly:

```bash
"/mnt/c/Program Files/CMake/bin/cmake.exe" --build build --config Release
```

This works because WSL interop can call `.exe` files and the build directory uses Windows paths internally.
