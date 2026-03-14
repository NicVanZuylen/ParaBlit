# ================================================================ BUILD STEPS =========================================================================
# Reflectron Pre-build step
if(WIN32)

add_custom_command(
    OUTPUT ${_BUILD_DIR_}/dummy2.adb
    DEPENDS Engine.Reflectron
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${_BUILD_DIR_}/$(Configuration)/Engine.Reflectron.exe -Path.SourceDir src/Engine.Runtime/
    COMMENT "Running Reflectron..."
)

elseif(LINUX)

add_custom_command(
    OUTPUT ${_BUILD_DIR_}/dummy2.adb
    DEPENDS Engine.Reflectron
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${_BUILD_DIR_}/Engine.Reflectron -Path.SourceDir src/Engine.Runtime/
    COMMENT "Running Reflectron..."
)

endif()

add_custom_target(
    PRE_BUILD
    DEPENDS ${_BUILD_DIR_}/dummy2.adb
)

if(WIN32)

# Pipeline Post-build step
add_custom_command(
    OUTPUT ${_BUILD_DIR_}/dummy.adb
    DEPENDS Engine.AssetPipeline
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${_BUILD_DIR_}/$(Configuration)/Engine.AssetPipeline.exe -Path.AssetsDir Assets/
    COMMENT "Running Build Pipeline..."
)

elseif(LINUX)

add_custom_command(
    OUTPUT ${_BUILD_DIR_}/dummy.adb
    DEPENDS Engine.AssetPipeline
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${_BUILD_DIR_}/Engine.AssetPipeline -Path.AssetsDir Assets/
    COMMENT "Running Build Pipeline..."
)

endif()

add_custom_target(
    POST_BUILD
    DEPENDS ${_BUILD_DIR_}/dummy.adb
)
# ======================================================================================================================================================
# =========================================================== UNIFIED RUNTIME SETUP ====================================================================
function(add_eng_runtime_library library_name)

# add the library
set(_LIBRARY_ ${library_name})

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

# Engine.AssetPipeline
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.AssetPipeline/include")
add_dependencies(${_LIBRARY_} Engine.AssetPipeline)

# Engine.Reflectron
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.Reflectron/include")
add_dependencies(${_LIBRARY_} Engine.Reflectron)

# Engine.Parablit
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.ParaBlit/include")
target_link_libraries(${_LIBRARY_} Engine.ParaBlit)
add_dependencies(${_LIBRARY_} Engine.ParaBlit)

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

# stb_image
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/stb")

# GLM
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/glm")

# Engine.Math
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.Math/include")

# Engine.Control
target_include_directories(${_LIBRARY_} PRIVATE "${_SOURCE_DIR_}/Engine.Control/include")
target_link_libraries(${_LIBRARY_} Engine.Control)
add_dependencies(${_LIBRARY_} Engine.Control)

# GLFW
set(_GLFW_BUILD_DIR_ "${_EXTERNAL_DIR_}/build/glfw/src/${CFG}")
target_include_directories(${_LIBRARY_} PRIVATE "${_EXTERNAL_DIR_}/glfw/include")
target_link_directories(${_LIBRARY_} PRIVATE "${_GLFW_BUILD_DIR_}")
target_link_libraries(${_LIBRARY_} glfw3)

# ImGUI
set(_IMGUI_SOURCE_DIR_ "${_EXTERNAL_DIR_}/imgui")

file(GLOB IMGUI_SRC_FILES
    "${_IMGUI_SOURCE_DIR_}/*.h"
    "${_IMGUI_SOURCE_DIR_}/*.cpp"
)
file(GLOB IMGUI_IMPL_SRC_FILES
    "${_IMGUI_SOURCE_DIR_}/backends/imgui_impl_glfw.h"
    "${_IMGUI_SOURCE_DIR_}/backends/imgui_impl_glfw.cpp"
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

# Working dir
set_target_properties(${_LIBRARY_} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

endif()

add_dependencies(${_LIBRARY_} PRE_BUILD)
add_dependencies(${_LIBRARY_} POST_BUILD)

# Output dir
set_target_properties(${_LIBRARY_} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_BUILD_DIR_}")
# --------------------------------------------------------------------------

# ------------------------- PREPROCESSOR DEFINITIONS -------------------------
add_compile_definitions(CLIB_ALLOCATOR_DUMP_ON_DESTRUCTION=1)

if(WIN32)
add_compile_definitions(ENG_WINDOWS)
add_compile_definitions(PARABLIT_WINDOWS)
add_compile_definitions(GLFW_EXPOSE_NATIVE_WIN32)
elseif(LINUX)
add_compile_definitions(ENG_LINUX)
add_compile_definitions(PARABLIT_LINUX)
add_compile_definitions(GLFW_EXPOSE_NATIVE_X11)
endif()

if(${_LIBRARY_} STREQUAL "Engine.Editor")
target_compile_definitions(${_LIBRARY_} PRIVATE ENG_EDITOR=1)
else()
target_compile_definitions(${_LIBRARY_} PRIVATE ENG_EDITOR=0)
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
target_include_directories(${_LIBRARY_} PRIVATE ${source_dir})

endfunction()
# ------------------------------------------------------------

# Source Files

add_source("src" "Engine.Runtime")
add_source("src/Main" "Main")
add_source("src/Utility" "Utility")
add_source("src/TinyObjLoader" "TinyObjLoader")
add_source("src/Resource" "Resource")
add_source("src/WorldRender" "WorldRender")
add_source("src/RenderGraph" "RenderGraph")
add_source("src/RenderGraphPasses" "RenderGraphPasses")
add_source("src/Entity" "Entity")
add_source("src/Entity/Component" "Entity/Component")
add_source("src/Editor" "Editor")

endfunction()
# ======================================================================================================================================================
# ========================================================== EDITOR + RUNTIME SETUP ====================================================================
add_eng_runtime_library(Engine.Runtime)
add_eng_runtime_library(Engine.Editor)
# ======================================================================================================================================================