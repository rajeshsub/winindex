@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=release

if /i "%CONFIG%"=="release" (
    cmake -B build\release -G "Visual Studio 18 2026" -A x64 && cmake --build build\release --config Release --parallel
) else if /i "%CONFIG%"=="debug" (
    cmake -B build\debug -G "Visual Studio 18 2026" -A x64 && cmake --build build\debug --config Debug --parallel
) else (
    echo Usage: build [release^|debug]
    exit /b 1
)
