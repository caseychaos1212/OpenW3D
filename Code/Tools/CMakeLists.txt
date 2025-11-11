# Top Level CMake for building SDK tools.
add_subdirectory(MakeMix)
add_subdirectory(RenRem)

add_subdirectory(MixViewer)
add_subdirectory(W3DView)
add_subdirectory(LevelEdit)
add_subdirectory(WWConfig)

if(W3D_BUILD_QT_TOOLS)
    add_subdirectory(WWConfigQt)
endif()
