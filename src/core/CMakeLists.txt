add_library(winindex_core STATIC
    indexer/DriveEnumerator.cpp
    indexer/MftScanner.cpp
    indexer/FindFileScanner.cpp
    indexer/UsnJournalMonitor.cpp
    indexer/ChangeWatcher.cpp
    indexer/Indexer.cpp
    search/SearchEngine.cpp
    search/SimdSearch.cpp
    search/SimdSearchAvx2.cpp
    search/TokenMatcher.cpp
    storage/IndexPool.cpp
    storage/IndexStore.cpp
    storage/IndexSerializer.cpp
    settings/Settings.cpp
    settings/PathUtils.cpp
    settings/Logger.cpp
)

target_include_directories(winindex_core
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(winindex_core
    PUBLIC  re2::re2 # So much faster than the std::regex so using this.
    PRIVATE ntdll
)

# SimdSearchAvx2.cpp must be compiled with AVX2 enabled so intrinsics are available.
# Runtime detection in SimdFindSubstring() guards actual execution on non-AVX2 CPUs.
if(MSVC)
    set_source_files_properties(search/SimdSearchAvx2.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
else()
    set_source_files_properties(search/SimdSearchAvx2.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
endif()

# Expose interfaces for mocking in tests
target_sources(winindex_core PUBLIC
    indexer/IFileSystemScanner.h
    indexer/IUsnJournalMonitor.h
    search/ISearchEngine.h
    storage/IIndexStore.h
)
