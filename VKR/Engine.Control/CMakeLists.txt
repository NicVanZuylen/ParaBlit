# add the library
set(_LIBRARY_ Engine.Control)

add_library(${_LIBRARY_} SHARED)
set_target_properties(${_LIBRARY_} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${_LIBRARY_} PROPERTIES OPTIMIZE_DEPENDENCIES TRUE)

# --------------------------- Dependencies ---------------------------

# CLib
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/CLib/ContainerLib/include")
add_dependencies(${_LIBRARY_} CLib)
target_link_libraries(${_LIBRARY_} CLib)
# --------------------------------------------------------------------

# --------------------------- Library Properties ---------------------------
if(MSVC)
set(CMAKE_CXX_FLAGS_MPCOMPILE "${CMAKE_CXX_FLAGS_MPCOMPILE} /MP")

# Multi-threaded compilation
set_target_properties(${_LIBRARY_} PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_MPCOMPILE}")

# Output dir
set_target_properties(${_LIBRARY_} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${_BUILD_DIR_})
set_target_properties(${_LIBRARY_} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${_BUILD_DIR_})
set_target_properties(${_LIBRARY_} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${_BUILD_DIR_})

endif()

target_include_directories(${_LIBRARY_} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/")

# --------------------------------------------------------------------------

# ------------------------- PREPROCESSOR DEFINITIONS -------------------------
add_compile_definitions(CONTROL_EXPORT)

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

add_source("src/Parse" "Parse")
add_source("src/pugixml" "pugixml")