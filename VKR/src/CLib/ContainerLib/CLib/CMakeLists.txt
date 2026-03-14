# add the library
set(_LIBRARY_ CLib)

add_library(${_LIBRARY_} SHARED)
set_target_properties(${_LIBRARY_} PROPERTIES LINKER_LANGUAGE CXX)

set(_INTERFACE_FILE_DIR_ "${_CLIB_SOURCE_DIR_}/include/")

if(MSVC)
set(CMAKE_CXX_FLAGS_MPCOMPILE "${CMAKE_CXX_FLAGS_MPCOMPILE} /MP")

set_target_properties(${_LIBRARY_} PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_MPCOMPILE}")
endif()

target_include_directories(${_LIBRARY_} PRIVATE ${_INTERFACE_FILE_DIR_})

# Output dir
set_target_properties(${_LIBRARY_} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${_CLIB_BUILD_DIR_})
set_target_properties(${_LIBRARY_} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${_CLIB_BUILD_DIR_})
set_target_properties(${_LIBRARY_} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${_CLIB_BUILD_DIR_})

# ------------------------- PREPROCESSOR DEFINITIONS -------------------------
add_compile_definitions(CLIB_EXPORT)

if (WIN32)
add_compile_definitions(CLIB_WINDOWS)
elseif (LINUX)
add_compile_definitions(CLIB_LINUX)
endif()

# ----------------------------------------------------------------------------

# --------------------------- FUNC ---------------------------
function(add_source source_dir filter_name)

file(GLOB_RECURSE SRC_FILES
    "${source_dir}/*.cpp"
)
source_group(${filter_name} FILES ${SRC_FILES})
target_sources(${_LIBRARY_} PRIVATE ${SRC_FILES})

endfunction()
# ------------------------------------------------------------

# Interface Files

file(GLOB_RECURSE INTERFACE_FILES
    "${_INTERFACE_FILE_DIR_}/Clib/*.h"
)

source_group(interface FILES ${INTERFACE_FILES})
target_sources(${_LIBRARY_} PUBLIC ${INTERFACE_FILES})

add_source("src/Allocators" "Allocators")
add_source("src/Reflection" "Reflection")