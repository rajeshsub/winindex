enable_testing()

add_executable(winindex_tests
    test_main.cpp
    test_IndexSerializer.cpp
    test_SearchEngine.cpp
    test_Settings.cpp
    test_DriveEnumerator.cpp
    test_Indexer.cpp
    mocks/MockFileSystemScanner.h
    mocks/MockUsnJournalMonitor.h
    mocks/MockIndexStore.h
)

target_link_libraries(winindex_tests
    PRIVATE
        winindex_core
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
)

target_include_directories(winindex_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

include(GoogleTest)
gtest_discover_tests(winindex_tests DISCOVERY_MODE PRE_TEST)
