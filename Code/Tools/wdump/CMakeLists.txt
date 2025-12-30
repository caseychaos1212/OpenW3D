set(WDUMP_SRC
    chunk_d.cpp
    FindDialog.cpp
    mainfrm.cpp
    rawfilem.cpp
    stdafx.cpp
    wdeview.cpp
    wdlview.cpp
    wdtview.cpp
    wdump.cpp
    wdumpdoc.cpp
    wdview.cpp
    wdump.rc
    res/wdump.rc2
    res/wdump.ico
    res/wdumpDoc.ico
    res/Toolbar.bmp
)

add_executable(wdump WIN32)

target_sources(wdump PRIVATE ${WDUMP_SRC})

target_include_directories(wdump
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../ww3d2
        ${CMAKE_CURRENT_SOURCE_DIR}/../../wwlib
        ${CMAKE_CURRENT_SOURCE_DIR}/../../WWMath
        ${CMAKE_CURRENT_SOURCE_DIR}/../WDumpCore
)

target_compile_definitions(wdump PRIVATE
    _AFXDLL
    G_CODE_BASE
    $<$<CONFIG:Debug>:_DEBUG>
)

# Link against the shared parsing core to keep behavior aligned with future Qt front-end.
target_link_libraries(wdump
    PRIVATE
        wdump_core
        wwlib
        wwcommon
        winmm
)

# Use shared MFC; required for Visual Studio generator to pull the right import libs.
set_property(TARGET wdump PROPERTY MFC_USE 2)
