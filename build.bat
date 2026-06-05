@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=release

if /i "%CONFIG%"=="release" (
    cmake -B build\release -G "Visual Studio 18 2026" -A x64 && cmake --build build\release --config Release --parallel
) else if /i "%CONFIG%"=="debug" (
    cmake -B build\debug -G "Visual Studio 18 2026" -A x64 && cmake --build build\debug --config Debug --parallel
) else if /i "%CONFIG%"=="asan" (
    cmake -B build\asan -G "Visual Studio 18 2026" -A x64 -DENABLE_ASAN=ON "-DCMAKE_CXX_FLAGS_DEBUG=/Ob0 /Od" && cmake --build build\asan --config Debug --parallel
) else if /i "%CONFIG%"=="clean" (
    if exist build\debug   rmdir /s /q build\debug
    if exist build\release rmdir /s /q build\release
    if exist build\asan    rmdir /s /q build\asan
) else if /i "%CONFIG%"=="all" (
    cmake -B build\debug   -G "Visual Studio 18 2026" -A x64 && cmake --build build\debug   --config Debug   --parallel
    cmake -B build\release -G "Visual Studio 18 2026" -A x64 && cmake --build build\release --config Release --parallel
    cmake -B build\asan    -G "Visual Studio 18 2026" -A x64 -DENABLE_ASAN=ON "-DCMAKE_CXX_FLAGS_DEBUG=/Ob0 /Od" && cmake --build build\asan --config Debug --parallel
) else (
    echo Usage: build [release^|debug^|asan^|all^|clean]
    exit /b 1
)

