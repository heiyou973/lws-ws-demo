cmake_minimum_required(VERSION 3.10)
project(lws_ws_demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(LWS_INSTALL_DIR "/tmp/opencode/lws-build/lws-install")

find_package(PkgConfig QUIET)

set(CMAKE_PREFIX_PATH ${LWS_INSTALL_DIR} ${CMAKE_PREFIX_PATH})

find_path(LWS_INCLUDE_DIR NAMES libwebsockets.h PATHS ${LWS_INSTALL_DIR}/include NO_DEFAULT_PATH)
find_library(LWS_LIBRARY NAMES websockets PATHS ${LWS_INSTALL_DIR}/lib NO_DEFAULT_PATH)

if(NOT LWS_INCLUDE_DIR OR NOT LWS_LIBRARY)
    message(FATAL_ERROR "libwebsockets not found")
endif()

message(STATUS "LWS_INCLUDE_DIR: ${LWS_INCLUDE_DIR}")
message(STATUS "LWS_LIBRARY: ${LWS_LIBRARY}")

add_executable(ws_server src/ws_server.c)
target_include_directories(ws_server PRIVATE ${LWS_INCLUDE_DIR})
target_link_libraries(ws_server PRIVATE ${LWS_LIBRARY} pthread)

add_executable(ws_client src/ws_client.c)
target_include_directories(ws_client PRIVATE ${LWS_INCLUDE_DIR})
target_link_libraries(ws_client PRIVATE ${LWS_LIBRARY} pthread)
