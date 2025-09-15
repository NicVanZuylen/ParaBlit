cmake_minimum_required(VERSION 3.10)

# set the project name
project(ContainerLib VERSION 1.0 LANGUAGES C CXX)

# set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(_CLIB_SOURCE_DIR_ ${CMAKE_CURRENT_SOURCE_DIR})

if (NOT DEFINED _CLIB_BUILD_DIR_)
set(_CLIB_BUILD_DIR_ "${CMAKE_CURRENT_SOURCE_DIR}/build")
endif()

add_subdirectory(CLib)
add_subdirectory(CLibClient)