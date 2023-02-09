cmake_minimum_required(VERSION 3.24)

# set the project name
project(NeuroverseEngine VERSION 1.0 LANGUAGES C CXX)

# set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# External libraries
set(LIBRARY_CLIB_DIR "${CMAKE_SOURCE_DIR}/External/CLib/ContainerLib")
set(LIBRARY_CLIB_CMAKE_LIBRARY_NAME CLib)

# Global Paths
set(_EXTERNAL_DIR_ "${CMAKE_SOURCE_DIR}/External")
set(_BUILD_DIR_ "${CMAKE_SOURCE_DIR}/build")

set(_VK_VERSION_ 1.2.198.1)

# External
add_subdirectory(${_EXTERNAL_DIR_})

add_subdirectory(Engine.Log)
add_subdirectory(Engine.ParaBlit)
add_subdirectory(Engine.Control)
add_subdirectory(Engine.AssetEncoder)
add_subdirectory(Engine.AssetPipeline)
add_subdirectory(Engine.Runtime)