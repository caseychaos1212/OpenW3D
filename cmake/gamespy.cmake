set(GS_OPENSSL FALSE)
set(GS_WINSOCK2 TRUE)
set(GAMESPY_SERVER_NAME "server.cnc-online.net")

set(W3D_GAMESPY_SOURCE_DIR "" CACHE PATH "Path to a local GamespySDK checkout")
if(W3D_GAMESPY_SOURCE_DIR)
    set(FETCHCONTENT_SOURCE_DIR_GAMESPY "${W3D_GAMESPY_SOURCE_DIR}")
endif()

if(DEFINED FETCHCONTENT_SOURCE_DIR_GAMESPY AND NOT FETCHCONTENT_SOURCE_DIR_GAMESPY STREQUAL "")
    if(NOT EXISTS "${FETCHCONTENT_SOURCE_DIR_GAMESPY}/CMakeLists.txt")
        message(FATAL_ERROR "FETCHCONTENT_SOURCE_DIR_GAMESPY points to '${FETCHCONTENT_SOURCE_DIR_GAMESPY}', but CMakeLists.txt was not found there.")
    endif()
elseif(W3D_FETCHCONTENT_OFFLINE)
    message(FATAL_ERROR "W3D_FETCHCONTENT_OFFLINE=ON requires a local GameSpy source. Set W3D_GAMESPY_SOURCE_DIR or FETCHCONTENT_SOURCE_DIR_GAMESPY.")
endif()

FetchContent_Declare(
    gamespy
    GIT_REPOSITORY https://github.com/TheAssemblyArmada/GamespySDK.git
    GIT_TAG        82258691a44a2aaebae787dbf9dfb872ecdcb237
)

FetchContent_MakeAvailable(gamespy)
