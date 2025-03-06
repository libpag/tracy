cmake_minimum_required(VERSION 3.13)

#include(../cmake/version.cmake)
#project(TracyServer LANGUAGES CXX VERSION ${TRACY_VERSION_STRING})
project(TracyServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(TRACY_PATH ${CMAKE_CURRENT_LIST_DIR}/..)


option(USE_LOCAL_PPQSORT "Use local ppqsort lib" ON)
include(${TRACY_PATH}/cmake/tracy.cmake)

set(TRACY_COMMON_DIR ${TRACY_PATH}/public/common)
set(TRACY_SERVER_DIR ${TRACY_PATH}/server)
set(TRACY_COMMON_SOURCES
        sha1.cpp
        WSSession.cpp
        tracy_lz4.cpp
        tracy_lz4hc.cpp
        TracySocket.cpp
        TracyStackFrames.cpp
        TracySystem.cpp
)

list(TRANSFORM TRACY_COMMON_SOURCES PREPEND "${TRACY_COMMON_DIR}/")
set(TRACY_SERVER_SOURCES
        TracyMemory.cpp
        TracyMmap.cpp
        TracyPrint.cpp
        TracySysUtil.cpp
        TracyTaskDispatch.cpp
        TracyTextureCompression.cpp
        TracyThreadCompress.cpp
        TracyWorker.cpp
)
list(TRANSFORM TRACY_SERVER_SOURCES PREPEND "${TRACY_SERVER_DIR}/")
list(APPEND TRACY_LIBS TracyCapstone TracyZstd)

add_library(TracyServer STATIC EXCLUDE_FROM_ALL ${TRACY_COMMON_SOURCES} ${TRACY_SERVER_SOURCES} ${SERVER_FILES} ${PROFILER_FILES} ${COMMON_FILES})
target_include_directories(TracyServer PUBLIC ${TRACY_COMMON_DIR} ${TRACY_SERVER_DIR})
target_link_libraries(TracyServer ${TRACY_LIBS})
