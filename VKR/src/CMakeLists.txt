cmake_minimum_required(VERSION 3.30)
project(NVEngine)

# Set build directories.
set(_CLIB_BUILD_DIR_ "${CMAKE_SOURCE_DIR}/build")

# Integrated third-party libraries
add_subdirectory(compressonator_core)
add_subdirectory(CLib)

# Add engine modules.
add_subdirectory(Engine.ParaBlit)
add_subdirectory(Engine.Math)
add_subdirectory(Engine.Control)
add_subdirectory(Engine.Reflectron)
add_subdirectory(Engine.AssetEncoder)
add_subdirectory(Engine.AssetPipeline)
add_subdirectory(Engine.Entity)
add_subdirectory(Engine.Runtime)