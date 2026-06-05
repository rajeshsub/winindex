cmake_minimum_required(VERSION 3.28)
project(winindex VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows-only
if(NOT WIN32)
    message(FATAL_ERROR "winindex targets Windows only.")
endif()

# Unicode; NOMINMAX prevents windows.h from defining min/max macros
add_compile_definitions(UNICODE _UNICODE NOMINMAX)

# ---------- Third-party via FetchContent ----------
# NOTE: MSVC strict flags are set AFTER FetchContent so third-party code
# doesn't inherit /WX and fail on their own alignment/padding warnings.
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# Do not install gtest alongside our project
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    absl
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG        20240722.0
)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(absl)

FetchContent_Declare(
    re2
    GIT_REPOSITORY https://github.com/google/re2.git
    GIT_TAG        2024-03-01
)
set(RE2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_GetProperties(re2)
if(NOT re2_POPULATED)
    FetchContent_Populate(re2)
    # re2's install(EXPORT) fails when absl targets aren't exported; strip it.
    file(READ "${re2_SOURCE_DIR}/CMakeLists.txt" _re2_cmake)
    string(REGEX REPLACE "install\\(EXPORT re2Targets[^)]*\\)" "" _re2_cmake "${_re2_cmake}")
    file(WRITE "${re2_SOURCE_DIR}/CMakeLists.txt" "${_re2_cmake}")
    add_subdirectory("${re2_SOURCE_DIR}" "${re2_BINARY_DIR}")
endif()

# MSVC-specific flags — applied after FetchContent so third-party code is unaffected
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /sdl /GS)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# ---------- Subdirectories ----------
add_subdirectory(src/core)
add_subdirectory(src/ui)
add_subdirectory(tests)

# ---------- Optional documentation ----------
option(BUILD_DOCS "Build Doxygen documentation" OFF)
if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        doxygen_add_docs(docs ${CMAKE_SOURCE_DIR}/src COMMENT "Generate API docs")
    endif()
endif()

# ---------- CPack / packaging ----------
# CPACK_* variables must be set before include(CPack)
set(CPACK_PACKAGE_NAME "winindex")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "winindex contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Blazing-fast Windows file search")
set(CPACK_GENERATOR "ZIP;NSIS")
set(CPACK_NSIS_DISPLAY_NAME "winindex")
set(CPACK_NSIS_MUI_ICON    "${CMAKE_SOURCE_DIR}/src/ui/winindex.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/src/ui/winindex.ico")
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\winindex\\\\winindex.lnk' '$INSTDIR\\\\bin\\\\winindex.exe'")
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\winindex\\\\winindex.lnk'")
include(CPack)
