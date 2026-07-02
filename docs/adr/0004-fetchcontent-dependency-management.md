Status: Accepted

## Context

winindex depends on three external C++ libraries: RE2, abseil-cpp, and GoogleTest.
A dependency management strategy is required.

## Options

| Option | Fits when | Cost now | Extension path | Trade-off |
|--------|-----------|----------|----------------|-----------|
| a. vcpkg | Large dep graph, shared toolchain | Install vcpkg + manifest | Broad package ecosystem | Requires vcpkg bootstrap; CI must cache vcpkg tree |
| b. Conan | Cross-platform build system integration | Install Conan + conanfile | Package recipes for most libs | Two-step configure; Conan 2 migration ongoing |
| c. CMake FetchContent | Small dep count, cmake-native | Zero (bundled with CMake 3.28) | Add more FetchContent blocks | Slower cold configure; each dep compiled from source |

## Decision

Use **CMake FetchContent** (option c) with pinned `GIT_TAG` values.

The dependency count is small (3 libs), all provide native CMake targets, and the
project already requires CMake 3.28+. FetchContent eliminates all pre-clone setup steps
- `cmake -B build` is the only command needed after cloning.

## Consequences

- Zero setup beyond CMake and a C++ compiler: `cmake -B build` just works.
- Cold configure fetches and compiles all deps (~30-60 s per build type first time).
- Subsequent configures use the cached `_deps` directory (CI caches by `CMakeLists.txt` hash).
- Deps are compiled per build type (debug/release/asan); disk usage is higher than a shared vcpkg install.
- Adding a dep requires a new `FetchContent_Declare` + `FetchContent_MakeAvailable` block.
