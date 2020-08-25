#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

struct VS_OUT
{
    vec2 texCoord;
    vec2 pad0;
};

layout(push_constant) uniform Bindings
{
    int mvpIndex;
} bindings;

layout(set = 1, binding = 0) uniform MVP
{
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp[];

layout (location = 0) out VS_OUT vsOutput;

void main() 
{
    mat4 model = mvp[nonuniformEXT(bindings.mvpIndex)].model;
    mat4 view = mvp[nonuniformEXT(bindings.mvpIndex)].view;
    mat4 proj = mvp[nonuniformEXT(bindings.mvpIndex)].proj;

    gl_Position = proj * view * model * inPosition;
    vsOutput.texCoord = inTexCoord;
}