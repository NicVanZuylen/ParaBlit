cmake_minimum_required(VERSION 3.24)

# set the project name
project(NeuroverseEngine VERSION 1.0 LANGUAGES C CXX)

# set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Global Paths
set(_EXTERNAL_DIR_ "${CMAKE_SOURCE_DIR}/External")
set(_BUILD_DIR_ "${CMAKE_SOURCE_DIR}/build")
set(_SOURCE_DIR_ "${CMAKE_SOURCE_DIR}/src")

set(_VK_VERSION_ 1.4.335.0)

if (WIN32)
set(_PLATFORM_ "win64")
elseif(LINUX)
set(_PLATFORM_ "linux")
else()
set(_PLATFORM_ "platform_unknown")
endif()

# External
# add_subdirectory(${_EXTERNAL_DIR_})

add_subdirectory(src)