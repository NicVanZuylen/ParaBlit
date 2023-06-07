#version 450
#include "Common/vertex_common.h"
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inPosition;

struct VS_OUT
{
    vec4 worldPos;
};

layout(push_constant) uniform Bindings
{
    int mvpIndex;
    int lightingUBOIdx;
} bindings;

layout(set = 1, binding = 0) uniform MVP
{
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;
    vec4 cameraPosition;
} mvp[];

struct PointLight
{
    vec4 position;
    vec3 color;
    float radius;
};

layout(set = 1, binding = 0) uniform PointLightBuffer
{
    PointLight m_lights[512];
} pointLightBuffer[];

//layout (location = 0) out VS_OUT vsOutput;
layout (location = 1) flat out int instanceIndex;

void main() 
{
    mat4 view = mvp[nonuniformEXT(bindings.mvpIndex)].view;
    mat4 proj = mvp[nonuniformEXT(bindings.mvpIndex)].proj;

    PointLight light = pointLightBuffer[nonuniformEXT(bindings.lightingUBOIdx)].m_lights[gl_InstanceIndex];

    vec3 finalPos = (inPosition * light.radius) + light.position.xyz;

    //vsOutput.worldPos = vec4(finalPos, 1.0);
    //gl_Position = proj * view * vsOutput.worldPos;
    gl_Position = proj * view * vec4(finalPos, 1.0);
    instanceIndex = gl_InstanceIndex;
}