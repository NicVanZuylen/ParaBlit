cmake_minimum_required(VERSION 3.24)

# set the project name
project(NVExternalDeps VERSION 1.0 LANGUAGES C CXX)

# set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# libraries

# glfw
add_subdirectory(glfw)

# meshoptimizer
add_subdirectory(meshoptimizer)

# shaderc
execute_process(COMMAND python git-sync-deps WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/shaderc/utils/")

set(SHADERC_ENABLE_TESTS OFF)
add_subdirectory(shaderc)
