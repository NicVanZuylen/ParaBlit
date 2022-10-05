# add the library
set(_LIBRARY_ Engine.AssetPipeline)

add_executable(${_LIBRARY_})
set_target_properties(${_LIBRARY_} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${_LIBRARY_} PROPERTIES OPTIMIZE_DEPENDENCIES TRUE)

# --------------------------- Dependencies ---------------------------

# Build output directory (.lib files used to link DLLs will appear here.)
target_link_directories(${_LIBRARY_} PRIVATE ${_BUILD_DIR_})

# CLib
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/CLib/ContainerLib/include")
target_link_libraries(${_LIBRARY_} CLib)

# Engine.AssetEncoder
target_include_directories(${_LIBRARY_} PRIVATE "${CMAKE_SOURCE_DIR}/Engine.AssetEncoder/include")
target_link_libraries(${_LIBRARY_} Engine.AssetEncoder)
add_dependencies(${_LIBRARY_} Engine.AssetEncoder)

# Engine.Parablit
target_include_directories(${_LIBRARY_} PRIVATE "${CMAKE_SOURCE_DIR}/Engine.ParaBlit/include")
target_link_libraries(${_LIBRARY_} Engine.ParaBlit)
add_dependencies(${_LIBRARY_} Engine.ParaBlit)
target_link_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/VulkanSDK/${_VK_VERSION_}/lib")
target_link_libraries(${_LIBRARY_} vulkan-1)

# stb_image
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/stb")

# GLM
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/glm")

# GLFW
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/glfw/include")
target_link_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/glfw/glfw-build/src/${CFG}")
target_link_libraries(${_LIBRARY_} glfw3)

# ShaderC
set(_SHADERC_BUILD_DIR_ "${_EXTERNAL_DIR_}/shaderc/shaderc-build/libshaderc/Release")
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/shaderc/libshaderc/include")
target_link_directories(${_LIBRARY_} PRIVATE "${_SHADERC_BUILD_DIR_}")
target_link_libraries(${_LIBRARY_} shaderc_shared)

# Compressonator
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/compressonator/src/cmp_compressonatorlib")
target_link_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/compressonator/src/compressonator-build/lib/${CFG}")
target_link_libraries(${_LIBRARY_} CMP_Compressonator)

# --------------------------------------------------------------------

# --------------------------- Library Properties ---------------------------
if(MSVC)
set(CMAKE_CXX_FLAGS_MPCOMPILE "${CMAKE_CXX_FLAGS_MPCOMPILE} /MP")

# Multi-threaded compilation
set_target_properties(${_LIBRARY_} PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_MPCOMPILE}")

# Output dir
set_target_properties(${_LIBRARY_} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${_BUILD_DIR_})

# Working dir
set_target_properties(${_LIBRARY_} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Copy Dependency DLLs
add_custom_command(
    TARGET ${_LIBRARY_}
    PRE_BUILD
    COMMAND \ncopy /y "\"${_SHADERC_BUILD_DIR_}/\\shaderc_shared.dll\"" "\"${_BUILD_DIR_}/$(Configuration)\""
    COMMENT "Copying dependent DLLs..."
)


endif()
# --------------------------------------------------------------------------

# ------------------------- PREPROCESSOR DEFINITIONS -------------------------
add_compile_definitions(PARABLIT_WINDOWS)
add_compile_definitions(GLFW_EXPOSE_NATIVE_WIN32)
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
add_source("src/pugixml" "pugixml")
add_source("src/bc7enc" "bc7enc")
add_source("src/TinyObjLoader" "TinyObjLoader")