# ================================================================ BUILD STEPS =========================================================================
# Reflectron Pre-build step

if(WIN32)

add_custom_command(
    OUTPUT ${_BUILD_DIR_}/AssetPipeline_reflectron_dummy.adb
    DEPENDS Engine.Reflectron
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${_BUILD_DIR_}/${Configuration}/Engine.Reflectron.exe -Path.SourceDir src/Engine.AssetPipeline/
    COMMENT "[Engine.AssetPipeline] Running Reflectron..."
)

elseif(LINUX)

add_custom_command(
    OUTPUT ${_BUILD_DIR_}/AssetPipeline_reflectron_dummy.adb
    DEPENDS Engine.Reflectron
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${_BUILD_DIR_}/Engine.Reflectron -Path.SourceDir src/Engine.AssetPipeline/
    COMMENT "[Engine.AssetPipeline] Running Reflectron..."
)

endif()

add_custom_target(
    PRE_BUILD_REFLECTRON
    DEPENDS ${_BUILD_DIR_}/AssetPipeline_reflectron_dummy.adb
)

# Copy Dependency DLLs
set(_COPY_DIR_ "${_EXTERNAL_DIR_}/build/shaderc/libshaderc/Release")
add_custom_command(
    OUTPUT ${_BUILD_DIR_}/AssetPipeline_copy_dlls_dummy.adb
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND \ncopy /y "\"${_COPY_DIR_}/\\shaderc_shared.dll\"" "\"${_BUILD_DIR_}/${Configuration}\""
    COMMENT "Copying dependent DLLs..."
)

add_custom_target(
    PRE_BUILD_COPY_DEPENDENT_DLLS
    DEPENDS ${_BUILD_DIR_}/AssetPipeline_copy_dlls_dummy.adb
)

# ======================================================================================================================================================

# add the library
set(_LIBRARY_ Engine.AssetPipeline)

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

# Engine.Reflectron
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.Reflectron/include")
add_dependencies(${_LIBRARY_} Engine.Reflectron)

# Engine.Parablit
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.ParaBlit/include")
target_link_libraries(${_LIBRARY_} Engine.ParaBlit)
add_dependencies(${_LIBRARY_} Engine.ParaBlit)

# Vulkan
if(WIN32)
target_link_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/VulkanSDK/${_PLATFORM_}/${_VK_VERSION_}/lib")
target_link_libraries(${_LIBRARY_} vulkan-1)
elseif(LINUX)
target_link_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/VulkanSDK/${_PLATFORM_}/${_VK_VERSION_}/x86_64/lib")
target_link_libraries(${_LIBRARY_} vulkan)
endif()

# stb_image
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/stb")

# GLM
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/glm")

# GLFW
set(_GLFW_BUILD_DIR_ "${_EXTERNAL_DIR_}/build/glfw/src/${CFG}")
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/glfw/include")
target_link_directories(${_LIBRARY_} PRIVATE "${_GLFW_BUILD_DIR_}")
target_link_libraries(${_LIBRARY_} glfw3)

# ShaderC
set(_SHADERC_BUILD_DIR_ "${_EXTERNAL_DIR_}/build/shaderc/libshaderc/Release")
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/shaderc/libshaderc/include")
target_link_directories(${_LIBRARY_} PRIVATE "${_SHADERC_BUILD_DIR_}")
target_link_libraries(${_LIBRARY_} shaderc_shared)

# Compressonator
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/compressonator_core/source")
target_link_libraries(${_LIBRARY_} CMP_Core)

# meshoptimizer
set(_MESHOPTIMIZER_BUILD_DIR_ "${_EXTERNAL_DIR_}/build/meshoptimizer/${CFG}")
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/meshoptimizer/src")
target_link_directories(${_LIBRARY_} PRIVATE "${_MESHOPTIMIZER_BUILD_DIR_}")
target_link_libraries(${_LIBRARY_} meshoptimizer)

# Engine.Math
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.Math/include")

# --------------------------------------------------------------------

# --------------------------- Library Properties ---------------------------
if(MSVC)
set(CMAKE_CXX_FLAGS_MPCOMPILE "${CMAKE_CXX_FLAGS_MPCOMPILE} /MP")

# Multi-threaded compilation
set_target_properties(${_LIBRARY_} PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_MPCOMPILE}")

# Working dir
set_target_properties(${_LIBRARY_} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Default command line arguments
set_target_properties(${_LIBRARY_} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "-Path.AssetsDir Assets/")

add_dependencies(${_LIBRARY_} PRE_BUILD_COPY_DEPENDENT_DLLS)
endif()

add_dependencies(${_LIBRARY_} PRE_BUILD_REFLECTRON)

# Output dir
set_target_properties(${_LIBRARY_} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_BUILD_DIR_}")
# --------------------------------------------------------------------------

# ------------------------- PREPROCESSOR DEFINITIONS -------------------------

if(WIN32)
add_compile_definitions(ASSETPIPELINE_WINDOWS)
add_compile_definitions(PARABLIT_WINDOWS)
add_compile_definitions(GLFW_EXPOSE_NATIVE_WIN32)
elseif(LINUX)
add_compile_definitions(ASSETPIPELINE_LINUX)
add_compile_definitions(PARABLIT_LINUX)
add_compile_definitions(GLFW_EXPOSE_NATIVE_X11)
endif()

# ----------------------------------------------------------------------------

# --------------------------- FUNC ---------------------------

# add_source
function(add_source source_dir filter_name)

file(GLOB_RECURSE SRC_FILES
    "${source_dir}/*.h"
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
add_source("src/bc6henc" "bc6henc")
add_source("src/TinyObjLoader" "TinyObjLoader")