#version 450
#extension GL_ARB_separate_shader_objects : enable
#include "Common/pb_common.h"

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform Bindings
{
    uint mvpIndex;
} PB_BINDINGS_NAME;

layout(set = 1, binding = 0) uniform MVP
{
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;
    vec4 cameraPosition;
} mvp[];

#define MVP_BUFFER PB_UBO(mvp, mvpIndex)

layout(location = 0) out vec4 outLineColor;

void main() 
{
    gl_Position = MVP_BUFFER.proj * MVP_BUFFER.view * inPosition;
    outLineColor = inColor;
}