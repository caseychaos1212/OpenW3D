set(LEVELEDIT_QT_SRC
    main.cpp
    MainWindow.cpp
    MainWindow.h
    Panels/PresetsPanel.cpp
    Panels/PresetsPanel.h
    Panels/InstancesPanel.cpp
    Panels/InstancesPanel.h
    Panels/ConversationsPanel.cpp
    Panels/ConversationsPanel.h
    Panels/OverlapPanel.cpp
    Panels/OverlapPanel.h
    Panels/HeightfieldPanel.cpp
    Panels/HeightfieldPanel.h
    Docks/OutputDock.cpp
    Docks/OutputDock.h
    Docks/AmbientLightDock.cpp
    Docks/AmbientLightDock.h
    Docks/CameraSettingsDock.cpp
    Docks/CameraSettingsDock.h
    CommandIds.h
    CommandRouter.cpp
    CommandRouter.h
    ActionStateModel.cpp
    ActionStateModel.h
    SettingsBridge.cpp
    SettingsBridge.h
    ConfigSettings.cpp
    ConfigSettings.h
    RuntimeSession.cpp
    RuntimeSession.h
    FeatureProfile.cpp
    FeatureProfile.h
    LevelEditViewport.cpp
    LevelEditViewport.h
    DeviceSelectionDialog.cpp
    DeviceSelectionDialog.h
    resources.qrc
)

add_executable(leveledit_qt WIN32)

target_sources(leveledit_qt PRIVATE ${LEVELEDIT_QT_SRC})

target_include_directories(leveledit_qt
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../LevelEdit
)

target_link_libraries(leveledit_qt
    PRIVATE
        qtcommon
        w3d_qt_toolkit
        wwcommon
        wwlib
)

if(WIN32)
    target_link_libraries(leveledit_qt PRIVATE
        shlwapi
        version
    )
endif()

set_target_properties(leveledit_qt PROPERTIES
    AUTOMOC ON
)

if(W3D_LEVELEDIT_QT_PROFILE STREQUAL "full")
    target_compile_definitions(leveledit_qt PRIVATE LEVELEDIT_QT_PROFILE_FULL)
else()
    target_compile_definitions(leveledit_qt PRIVATE LEVELEDIT_QT_PROFILE_PUBLIC)
endif()

if(W3D_LEVELEDIT_GIT_SCM)
    target_compile_definitions(leveledit_qt PRIVATE W3D_LEVELEDIT_GIT_SCM)
endif()

if(W3D_LEVELEDIT_DDB_JSON_MIRROR)
    target_compile_definitions(leveledit_qt PRIVATE W3D_LEVELEDIT_DDB_JSON_MIRROR)
endif()
