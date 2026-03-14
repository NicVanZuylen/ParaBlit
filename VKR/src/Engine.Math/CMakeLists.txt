# add the library
set(_LIBRARY_ Engine.Math)

add_library(${_LIBRARY_} SHARED)
set_target_properties(${_LIBRARY_} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${_LIBRARY_} PROPERTIES OPTIMIZE_DEPENDENCIES TRUE)

# --------------------------- Dependencies ---------------------------
# GLM
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/glm")

# ImGUI
set(_IMGUI_SOURCE_DIR_ "${_EXTERNAL_DIR_}/imgui")

file(GLOB IMGUI_SRC_FILES
    "${_IMGUI_SOURCE_DIR_}/*.h"
    "${_IMGUI_SOURCE_DIR_}/*.cpp"
)
file(GLOB IMGUI_IMPL_SRC_FILES
    "${_IMGUI_SOURCE_DIR_}/backends/imgui_impl_vulkan.h"
    "${_IMGUI_SOURCE_DIR_}/backends/imgui_impl_vulkan.cpp"
)
source_group("ImGUI" FILES ${IMGUI_SRC_FILES})
target_sources(${_LIBRARY_} PRIVATE ${IMGUI_SRC_FILES})
source_group("ImGUI/Impl" FILES ${IMGUI_IMPL_SRC_FILES})
target_sources(${_LIBRARY_} PRIVATE ${IMGUI_IMPL_SRC_FILES})
target_include_directories(${_LIBRARY_} PRIVATE ${_IMGUI_SOURCE_DIR_})

# --------------------------------------------------------------------


# --------------------------- Library Properties ---------------------------
if(MSVC)
set(CMAKE_CXX_FLAGS_MPCOMPILE "${CMAKE_CXX_FLAGS_MPCOMPILE} /MP")

# Multi-threaded compilation
set_target_properties(${_LIBRARY_} PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_MPCOMPILE}")

endif()

target_include_directories(${_LIBRARY_} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/")

# Output dir
set_target_properties(${_LIBRARY_} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_BUILD_DIR_}")
set_target_properties(${_LIBRARY_} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${_BUILD_DIR_}")
set_target_properties(${_LIBRARY_} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${_BUILD_DIR_}")

# --------------------------------------------------------------------------

# ------------------------- PREPROCESSOR DEFINITIONS -------------------------
add_compile_definitions(MATH_EXPORT)

# ----------------------------------------------------------------------------

# --------------------------- FUNC ---------------------------
function(add_source source_dir filter_name)

file(GLOB_RECURSE SRC_FILES
    "${source_dir}/*.h"
    "${source_dir}/*.hpp"
    "${source_dir}/*.c"
    "${source_dir}/*.cpp"
    "${source_dir}/*.ixx"
)
source_group(${filter_name} FILES ${SRC_FILES})
target_sources(${_LIBRARY_} PRIVATE ${SRC_FILES})

endfunction()
# ------------------------------------------------------------

# Interface Files

set(_INCLUDE_DIR_ "${CMAKE_CURRENT_SOURCE_DIR}/include/${_LIBRARY_}")
file(GLOB_RECURSE INTERFACE_FILES
    "${_INCLUDE_DIR_}/*.h"
)

source_group(interface FILES ${INTERFACE_FILES})
target_sources(${_LIBRARY_} PRIVATE ${INTERFACE_FILES})

# Source Files

add_source("src" "${_LIBRARY_}")