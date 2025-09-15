# add the library
set(_LIBRARY_ Engine.Entity)

add_library(${_LIBRARY_} SHARED)
set_target_properties(${_LIBRARY_} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${_LIBRARY_} PROPERTIES OPTIMIZE_DEPENDENCIES TRUE)

# --------------------------- Dependencies ---------------------------

# CLib
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/CLib/ContainerLib/include")
add_dependencies(${_LIBRARY_} CLib)
target_link_libraries(${_LIBRARY_} CLib)

# Engine.Control
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.Control/include")
target_link_libraries(${_LIBRARY_} Engine.Control)
add_dependencies(${_LIBRARY_} Engine.Control)

# Engine.Math
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.Math/include")
# --------------------------------------------------------------------

# --------------------------- Library Properties ---------------------------
if(MSVC)
set(CMAKE_CXX_FLAGS_MPCOMPILE "${CMAKE_CXX_FLAGS_MPCOMPILE} /MP")

# Multi-threaded compilation
set_target_properties(${_LIBRARY_} PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_MPCOMPILE}")

# Output dir
set_target_properties(${_LIBRARY_} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_BUILD_DIR_}")
set_target_properties(${_LIBRARY_} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${_BUILD_DIR_}")
set_target_properties(${_LIBRARY_} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${_BUILD_DIR_}")

endif()

target_include_directories(${_LIBRARY_} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/")
target_include_directories(${_LIBRARY_} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/")

# --------------------------------------------------------------------------

# ------------------------- PREPROCESSOR DEFINITIONS -------------------------
add_compile_definitions(CONTROL_EXPORT)

# ----------------------------------------------------------------------------

# --------------------------- FUNC ---------------------------
function(add_source source_dir filter_name)

file(GLOB SRC_FILES
    "${source_dir}/*.h"
    "${source_dir}/*.hpp"
    "${source_dir}/*.c"
    "${source_dir}/*.cpp"
    "${source_dir}/*.ixx"
)

source_group(${filter_name} FILES ${SRC_FILES})
target_sources(${_LIBRARY_} PUBLIC ${SRC_FILES})

endfunction()
# ------------------------------------------------------------

# Interface Files

file(GLOB_RECURSE INTERFACE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/${_LIBRARY_}/*.h"
)

source_group(interface FILES ${INTERFACE_FILES})
target_sources(${_LIBRARY_} PRIVATE ${INTERFACE_FILES})

# Source Files

add_source("src/Entity" "Entity")
add_source("src/Entity/Component" "Entity/Component")