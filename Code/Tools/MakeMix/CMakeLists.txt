add_executable(makemix MakeMix.cpp)

target_link_libraries(makemix PRIVATE wwcommon wwdebug wwlib)

if(WIN32)
    target_link_libraries(makemix PRIVATE winmm)
endif()
