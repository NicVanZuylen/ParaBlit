# add the library
set(_LIBRARY_ Engine.ParaBlit)

add_library(${_LIBRARY_} SHARED)
set_target_properties(${_LIBRARY_} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${_LIBRARY_} PROPERTIES OPTIMIZE_DEPENDENCIES TRUE)

# --------------------------- Dependencies ---------------------------

# Vulkan
if(WIN32)
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/VulkanSDK/${_PLATFORM_}/${_VK_VERSION_}/include")
target_link_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/VulkanSDK/${_PLATFORM_}/${_VK_VERSION_}/lib")
target_link_libraries(${_LIBRARY_} vulkan-1)
elseif(LINUX)
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/VulkanSDK/${_PLATFORM_}/${_VK_VERSION_}/x86_64/include")
target_link_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/VulkanSDK/${_PLATFORM_}/${_VK_VERSION_}/x86_64/lib")
target_link_libraries(${_LIBRARY_} vulkan)
endif()

# CLib
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/CLib/ContainerLib/include")
target_link_libraries(${_LIBRARY_} CLib)

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

# Output dir
set_target_properties(${_LIBRARY_} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_BUILD_DIR_}")
set_target_properties(${_LIBRARY_} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${_BUILD_DIR_}")
set_target_properties(${_LIBRARY_} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${_BUILD_DIR_}")
# --------------------------------------------------------------------------

# ------------------------- PREPROCESSOR DEFINITIONS -------------------------
add_compile_definitions(PARABLIT_EXPORT)
add_compile_definitions(PB_FRAME_IN_FLIGHT_COUNT=3)

if(WIN32)
add_compile_definitions(PARABLIT_WINDOWS)
elseif(LINUX)
add_compile_definitions(PARABLIT_LINUX)
add_compile_definitions(PARABLIT_ENABLE_API_VALIDATION)
add_compile_definitions(PB_USE_DEBUG_UTILS)
endif()

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
add_source("src/ImGUIModule" "ImGUIModule")