include_guard(GLOBAL)

if(TARGET w3d_qt_toolkit)
    return()
endif()

set(W3D_QT_MIN_VERSION "6.6" CACHE STRING "Minimum Qt version to search for")
set(_w3d_qt_components Core Gui Widgets Svg OpenGLWidgets)
set(_w3d_qt_optional_components LinguistTools)

find_package(Qt6 ${W3D_QT_MIN_VERSION} COMPONENTS ${_w3d_qt_components} OPTIONAL_COMPONENTS ${_w3d_qt_optional_components} QUIET)

if(Qt6_FOUND)
    set(W3D_QT_PACKAGE "Qt6" CACHE STRING "Qt package namespace" FORCE)
    set(W3D_QT_VERSION "${Qt6_VERSION}" CACHE STRING "Detected Qt version" FORCE)
else()
    find_package(Qt5 5.15 COMPONENTS ${_w3d_qt_components} OPTIONAL_COMPONENTS ${_w3d_qt_optional_components} REQUIRED)
    set(W3D_QT_PACKAGE "Qt5" CACHE STRING "Qt package namespace" FORCE)
    set(W3D_QT_VERSION "${Qt5_VERSION}" CACHE STRING "Detected Qt version" FORCE)
endif()

if(W3D_QT_PACKAGE STREQUAL "Qt6")
    set(_w3d_qt_libraries
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Svg
        Qt6::OpenGLWidgets
    )
else()
    set(_w3d_qt_libraries
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Svg
        Qt5::OpenGLWidgets
    )
endif()

add_library(w3d_qt_toolkit INTERFACE)
target_link_libraries(w3d_qt_toolkit INTERFACE ${_w3d_qt_libraries})

# Expose component list for downstream targets that need to request optional Qt modules.
set(W3D_QT_COMPONENTS ${_w3d_qt_components} CACHE INTERNAL "Qt components enabled for W3D")
