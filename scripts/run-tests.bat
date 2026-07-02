@echo off
if not exist build\debug\CMakeCache.txt (
    cmake --preset windows-msvc-debug
    if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
)
cmake --build build\debug --config Debug --parallel
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
ctest --test-dir build\debug -C Debug --output-on-failure
