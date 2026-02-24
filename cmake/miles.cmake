set(W3D_MILES_SOURCE_DIR "" CACHE PATH "Path to a local miles-sdk-stub checkout")
if(W3D_MILES_SOURCE_DIR)
    set(FETCHCONTENT_SOURCE_DIR_MILES "${W3D_MILES_SOURCE_DIR}")
endif()

if(DEFINED FETCHCONTENT_SOURCE_DIR_MILES AND NOT FETCHCONTENT_SOURCE_DIR_MILES STREQUAL "")
    if(NOT EXISTS "${FETCHCONTENT_SOURCE_DIR_MILES}/CMakeLists.txt")
        message(FATAL_ERROR "FETCHCONTENT_SOURCE_DIR_MILES points to '${FETCHCONTENT_SOURCE_DIR_MILES}', but CMakeLists.txt was not found there.")
    endif()
elseif(W3D_FETCHCONTENT_OFFLINE)
    message(FATAL_ERROR "W3D_FETCHCONTENT_OFFLINE=ON requires a local Miles stub source. Set W3D_MILES_SOURCE_DIR or FETCHCONTENT_SOURCE_DIR_MILES.")
endif()

FetchContent_Declare(
    miles
    GIT_REPOSITORY https://github.com/TheSuperHackers/miles-sdk-stub.git
    GIT_TAG        ff364dd3308a7c3470188427cfa481fe7d993551
)

set(MILES_NOFLOAT TRUE)
FetchContent_MakeAvailable(miles)
