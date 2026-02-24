set(W3D_BINK_SOURCE_DIR "" CACHE PATH "Path to a local bink-sdk-stub checkout")
if(W3D_BINK_SOURCE_DIR)
    set(FETCHCONTENT_SOURCE_DIR_BINK "${W3D_BINK_SOURCE_DIR}")
endif()

if(DEFINED FETCHCONTENT_SOURCE_DIR_BINK AND NOT FETCHCONTENT_SOURCE_DIR_BINK STREQUAL "")
    if(NOT EXISTS "${FETCHCONTENT_SOURCE_DIR_BINK}/CMakeLists.txt")
        message(FATAL_ERROR "FETCHCONTENT_SOURCE_DIR_BINK points to '${FETCHCONTENT_SOURCE_DIR_BINK}', but CMakeLists.txt was not found there.")
    endif()
elseif(W3D_FETCHCONTENT_OFFLINE)
    message(FATAL_ERROR "W3D_FETCHCONTENT_OFFLINE=ON requires a local Bink stub source. Set W3D_BINK_SOURCE_DIR or FETCHCONTENT_SOURCE_DIR_BINK, or disable Bink.")
endif()

FetchContent_Declare(
    bink
    GIT_REPOSITORY https://github.com/TheSuperHackers/bink-sdk-stub.git
    GIT_TAG        3241ee1e3739b21d9c0a0760c1a5d5622d21c093
)

FetchContent_MakeAvailable(bink)
