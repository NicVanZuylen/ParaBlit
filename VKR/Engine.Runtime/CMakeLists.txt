# add the library
set(_LIBRARY_ Engine.Runtime)

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

# Engine.AssetPipeline
target_include_directories(${_LIBRARY_} PRIVATE "${CMAKE_SOURCE_DIR}/Engine.AssetPipeline/include")
add_dependencies(${_LIBRARY_} Engine.AssetPipeline)

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

# Pipeline Post-build step
add_custom_command(
    OUTPUT ${_BUILD_DIR_}/dummy.adb
    DEPENDS Engine.AssetPipeline
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${_BUILD_DIR_}/$(Configuration)/Engine.AssetPipeline.exe -Path.AssetsDir Assets/
    COMMENT "Running Build Pipeline..."
)

add_custom_target(
    POST_BUILD
    DEPENDS ${_BUILD_DIR_}/dummy.adb
)

add_dependencies(${_LIBRARY_} POST_BUILD)

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
target_include_directories(${_LIBRARY_} PRIVATE ${source_dir})

endfunction()
# ------------------------------------------------------------

# Source Files

add_source("src" "${_LIBRARY_}")
add_source("src/Main" "Main")
add_source("src/Utility" "Utility")
add_source("src/TinyObjLoader" "TinyObjLoader")
add_source("src/Resource" "Resource")
add_source("src/World" "World")
add_source("src/RenderGraph" "RenderGraph")
add_source("src/RenderGraphPasses" "RenderGraphPasses")