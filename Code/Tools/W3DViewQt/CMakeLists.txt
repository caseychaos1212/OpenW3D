set(W3DVIEW_QT_SRC
    main.cpp
    MainWindow.cpp
    MainWindow.h
    AggregateNameDialog.cpp
    AggregateNameDialog.h
    AddToLineupDialog.cpp
    AddToLineupDialog.h
    AdvancedAnimationDialog.cpp
    AdvancedAnimationDialog.h
    AnimationPropertiesDialog.cpp
    AnimationPropertiesDialog.h
    AnimatedSoundOptionsDialog.cpp
    AnimatedSoundOptionsDialog.h
    BackgroundObjectDialog.cpp
    BackgroundObjectDialog.h
    BoneManagementDialog.cpp
    BoneManagementDialog.h
    CameraDistanceDialog.cpp
    CameraDistanceDialog.h
    CameraSettingsDialog.cpp
    CameraSettingsDialog.h
    EmitterEditDialog.cpp
    EmitterEditDialog.h
    GammaDialog.cpp
    GammaDialog.h
    KeyframeTableUtils.cpp
    KeyframeTableUtils.h
    HierarchyPropertiesDialog.cpp
    HierarchyPropertiesDialog.h
    RenderObjUtils.cpp
    RenderObjUtils.h
    MeshPropertiesDialog.cpp
    MeshPropertiesDialog.h
    OpacityVectorEditDialog.cpp
    OpacityVectorEditDialog.h
    RingEditDialog.cpp
    RingEditDialog.h
    PlaySoundDialog.cpp
    PlaySoundDialog.h
    ScaleDialog.cpp
    ScaleDialog.h
    SphereEditDialog.cpp
    SphereEditDialog.h
    SoundEditDialog.cpp
    SoundEditDialog.h
    TexturePathDialog.cpp
    TexturePathDialog.h
    W3DViewport.cpp
    W3DViewport.h
)

add_executable(w3dview_qt WIN32)

target_sources(w3dview_qt PRIVATE ${W3DVIEW_QT_SRC})

target_include_directories(w3dview_qt
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ../../wwdebug
        ../../wwlib
        ../../ww3d2
        ../../WWMath
        ../../wwphys
        ../../wwsaveload
        ../../WWAudio
)

target_link_libraries(w3dview_qt
    PRIVATE
        qtcommon
        w3d_qt_toolkit
        d3d9lib
        version
        winmm
        wwcommon
        wwdebug
        wwlib
        wwmath
        wwphys
        wwsaveload
        ww3d2
        wwaudio
)

set_target_properties(w3dview_qt PROPERTIES
    AUTOMOC ON
)
