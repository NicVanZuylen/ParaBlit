# add the library
set(_LIBRARY_ CLibClient)

add_executable(${_LIBRARY_})
set_target_properties(${_LIBRARY_} PROPERTIES LINKER_LANGUAGE CXX)

add_dependencies(${_LIBRARY_} CLib)
target_link_libraries(${_LIBRARY_} CLib)

# Output dir
set_target_properties(${_LIBRARY_} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${_CLIB_BUILD_DIR_})
set_target_properties(${_LIBRARY_} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${_CLIB_BUILD_DIR_})
set_target_properties(${_LIBRARY_} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${_CLIB_BUILD_DIR_})

# --------------------------- FUNC ---------------------------
function(add_source source_dir filter_name)

file(GLOB_RECURSE SRC_FILES
    "${source_dir}/*.cpp"
    "${source_dir}/*.hpp"
)
source_group(${filter_name} FILES ${SRC_FILES})
target_sources(CLib PRIVATE ${SRC_FILES})

endfunction()
# ------------------------------------------------------------

if(MSVC)
set(CMAKE_CXX_FLAGS_MPCOMPILE "${CMAKE_CXX_FLAGS_MPCOMPILE} /MP")

set_target_properties(${_LIBRARY_} PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_MPCOMPILE}")
endif()

include_directories("${_CLIB_SOURCE_DIR_}/include/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")

target_sources(${_LIBRARY_} PRIVATE 
main.cpp
AllocatorTests.h
FixedBlockAllocatorTests.h
SubAllocatorTests.h
VectorTests.h
ReflectionTests.h
)

add_source("boost" "boost")