cmake_minimum_required(VERSION 2.8)
project(watcherServer)

#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/usr/local/etc/cmake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
add_definitions( -DTIXML_USE_STL )

set(CMAKE_CXX_STANDARD 20)

set(SRCFILES server.cc)

add_executable(watcherServer.exe ${SRCFILES})
