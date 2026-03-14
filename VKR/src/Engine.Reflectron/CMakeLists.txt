# add the library
set(_LIBRARY_ Engine.Reflectron)

add_executable(${_LIBRARY_})
set_target_properties(${_LIBRARY_} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${_LIBRARY_} PROPERTIES OPTIMIZE_DEPENDENCIES TRUE)

# --------------------------- Dependencies ---------------------------

# Build output directory (.lib files used to link DLLs will appear here.)
target_link_directories(${_LIBRARY_} PRIVATE ${_BUILD_DIR_})

# CLib
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/CLib/ContainerLib/include")
target_link_libraries(${_LIBRARY_} CLib)

# Engine.AssetEncoder
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.AssetEncoder/include")
target_link_libraries(${_LIBRARY_} Engine.AssetEncoder)
add_dependencies(${_LIBRARY_} Engine.AssetEncoder)

# Engine.Control
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.Control/include")
target_link_libraries(${_LIBRARY_} Engine.Control)
add_dependencies(${_LIBRARY_} Engine.Control)

# --------------------------------------------------------------------

# --------------------------- Library Properties ---------------------------
if(MSVC)
set(CMAKE_CXX_FLAGS_MPCOMPILE "${CMAKE_CXX_FLAGS_MPCOMPILE} /MP")

# Multi-threaded compilation
set_target_properties(${_LIBRARY_} PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_MPCOMPILE}")

# Working dir
set_target_properties(${_LIBRARY_} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Default command line arguments
set_target_properties(${_LIBRARY_} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "")

endif()

# Output dir
set_target_properties(${_LIBRARY_} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_BUILD_DIR_}")

# --------------------------------------------------------------------------

# ------------------------- PREPROCESSOR DEFINITIONS -------------------------

if (WIN32)
add_compile_definitions(REFLECTRON_WINDOWS)
add_compile_definitions(GLFW_EXPOSE_NATIVE_WIN32)
elseif(LINUX)
add_compile_definitions(REFLECTRON_LINUX)
endif()
# ----------------------------------------------------------------------------

# --------------------------- FUNC ---------------------------

# add_source
function(add_source source_dir filter_name)

file(GLOB SRC_FILES
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
target_include_directories(${_LIBRARY_} PRIVATE ${_INCLUDE_DIR_})

# Source Files

add_source("src" "${_LIBRARY_}")
add_source("src/Encode" "Encode")
add_source("src/bc7enc" "bc7enc")
add_source("src/TinyObjLoader" "TinyObjLoader")