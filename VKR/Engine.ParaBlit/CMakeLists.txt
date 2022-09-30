# add the library
set(_LIBRARY_ Engine.ParaBlit)

add_library(${_LIBRARY_} SHARED)
set_target_properties(${_LIBRARY_} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${_LIBRARY_} PROPERTIES OPTIMIZE_DEPENDENCIES TRUE)

# --------------------------- Dependencies ---------------------------

# Vulkan
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/VulkanSDK/${_VK_VERSION_}/include")
target_link_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/VulkanSDK/${_VK_VERSION_}/lib")
target_link_libraries(${_LIBRARY_} vulkan-1)

# CLib
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/CLib/ContainerLib/include")
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
# --------------------------------------------------------------------------

# ------------------------- PREPROCESSOR DEFINITIONS -------------------------
add_compile_definitions(PARABLIT_EXPORT)
add_compile_definitions(PB_FRAME_IN_FLIGHT_COUNT=3)
add_compile_definitions(PARABLIT_WINDOWS)

target_compile_definitions(${_LIBRARY_} PRIVATE
  $<$<CONFIG:Debug>:
  PARABLIT_ENABLE_API_VALIDATION
  PB_USE_DEBUG_UTILS
  >
)

# ----------------------------------------------------------------------------

# --------------------------- FUNC ---------------------------

# add_source
function(add_source source_dir filter_name)

file(GLOB_RECURSE SRC_FILES
    "${source_dir}/*.h"
    "${source_dir}/*.cpp"
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
add_source("src/MurmurHash" "MurmurHash")