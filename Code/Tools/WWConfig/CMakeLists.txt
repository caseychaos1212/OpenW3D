# Core locale/IO helpers shared with both UIs.
set(WWCONFIG_CORE_SRC
    EZGIMEX.cpp
    locale.cpp
    Locale_API.cpp
    gimex.h
    locale.h
    Locale_API.h
    WWConfigSettings.cpp
    WWConfigSettings.h
)

add_library(wwconfig_core STATIC)

target_sources(wwconfig_core PRIVATE ${WWCONFIG_CORE_SRC})

target_include_directories(wwconfig_core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Code/wwlib
)

target_link_libraries(wwconfig_core
    PUBLIC
        wwcommon
        wwlib
        ww3d2
)

set(WWCONFIG_SRC
    AudioConfigDialog.cpp
    DriverVersionWarning.cpp
    PerformanceConfigDialog.cpp
    StdAfx.cpp
    VideoConfigDialog.cpp
    WWConfig.cpp
    WWConfigDlg.cpp
    AudioConfigDialog.h
    DriverVersionWarning.h
    PerformanceConfigDialog.h
    Resource.h
    StdAfx.h
    VideoConfigDialog.h
    WWConfig.h
    WWConfigDlg.h
    WWConfig.rc
)

# Targets to build.
add_executable(wwconfig WIN32)

target_compile_definitions(wwconfig PRIVATE
    _AFXDLL
)

target_link_libraries(wwconfig PRIVATE
    version
    winmm
    wwconfig_core
    wwcommon
    wwdebug
    wwmath
    wwphys
    wwsaveload
    ww3d2
    wwaudio
)

target_sources(wwconfig PRIVATE ${WWCONFIG_SRC})

if(MSVC)
    target_link_options(wwconfig PRIVATE "/NODEFAULTLIB:libci.lib")
endif()
