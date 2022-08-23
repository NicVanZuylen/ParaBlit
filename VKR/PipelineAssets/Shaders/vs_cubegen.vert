#version 450
#include "Common/pb_common.h"
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Bindings
{
    uint constantsIndex;
} PB_BINDINGS_NAME;

layout(set = 1, binding = 0) uniform CubeGenConstantsLayout
{
    mat4 proj;
    mat4 view;
} constants[];

#define CONSTANTS PB_UBO(constants, constantsIndex)

layout (location = 0) out vec3 outPosition;

// Source: https://learnopengl.com/code_viewer.php?code=advanced/cubemaps_skybox_data
vec3 skyboxVertices[] = 
{        
    vec3(-1.0,  1.0, -1.0),
    vec3(-1.0, -1.0, -1.0),
    vec3(1.0, -1.0, -1.0),
    vec3(1.0, -1.0, -1.0),
    vec3(1.0,  1.0, -1.0),
    vec3(-1.0,  1.0, -1.0),

    vec3(-1.0, -1.0,  1.0),
    vec3(-1.0, -1.0, -1.0),
    vec3(-1.0,  1.0, -1.0),
    vec3(-1.0,  1.0, -1.0),
    vec3(-1.0,  1.0,  1.0),
    vec3(-1.0, -1.0,  1.0),

    vec3(1.0, -1.0, -1.0),
    vec3(1.0, -1.0,  1.0),
    vec3(1.0,  1.0,  1.0),
    vec3(1.0,  1.0,  1.0),
    vec3(1.0,  1.0, -1.0),
    vec3(1.0, -1.0, -1.0),

    vec3(-1.0, -1.0,  1.0),
    vec3(-1.0,  1.0,  1.0),
    vec3(1.0,  1.0,  1.0),
    vec3(1.0,  1.0,  1.0),
    vec3(1.0, -1.0,  1.0),
    vec3(-1.0, -1.0,  1.0),

    vec3(-1.0,  1.0, -1.0),
    vec3(1.0,  1.0, -1.0),
    vec3(1.0,  1.0,  1.0),
    vec3(1.0,  1.0,  1.0),
    vec3(-1.0,  1.0,  1.0),
    vec3(-1.0,  1.0, -1.0),

    vec3(-1.0, -1.0, -1.0),
    vec3(-1.0, -1.0,  1.0),
    vec3(1.0, -1.0, -1.0),
    vec3(1.0, -1.0, -1.0),
    vec3(-1.0, -1.0,  1.0),
    vec3(1.0, -1.0,  1.0)
};

void main() 
{
    outPosition = skyboxVertices[gl_VertexIndex];
    gl_Position = CONSTANTS.proj * CONSTANTS.view * vec4(outPosition, 1.0);
    gl_Position = gl_Position.xyww;
}