#version 450
#include "Common/pb_common.h"
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Bindings
{
    uint mvpUBOIndex;
} PB_BINDINGS_NAME;

layout(set = 1, binding = 0) uniform MVPLayout
{
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;
    vec4 cameraPosition;
} mvp[];

#define MVP PB_UBO(mvp, mvpUBOIndex)

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
    mat4 view = MVP.view;
    view[3] = vec4(0.0, 0.0, 0.0, 1.0);

    outPosition = skyboxVertices[gl_VertexIndex];
    gl_Position = MVP.proj * view * vec4(outPosition, 1.0);
    gl_Position = gl_Position.xyww;
}