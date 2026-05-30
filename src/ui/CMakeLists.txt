add_executable(winindex WIN32
    main.cpp
    MainWindow.cpp
    MainWindow.h
    FirstRunDialog.cpp
    FirstRunDialog.h
    SettingsDialog.cpp
    SettingsDialog.h
    resource.h
    winindex.rc
)

target_link_libraries(winindex
    PRIVATE
        winindex_core
        comctl32
        shell32
        shlwapi
)

target_include_directories(winindex PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
)

# Manifest for Common Controls v6 (visual styles)
set_target_properties(winindex PROPERTIES
    LINK_FLAGS "/MANIFEST:EMBED /MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/winindex.manifest"
)

install(TARGETS winindex DESTINATION bin)
