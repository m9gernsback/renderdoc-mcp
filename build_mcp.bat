@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d E:\GitHub\renderdoc-mcp

echo === Rebuilding renderdoc-mcp ===
cmake --build build --config Release
if errorlevel 1 exit /b 1

echo === Running unit tests ===
ctest --test-dir build -L unit -C Release --output-on-failure
