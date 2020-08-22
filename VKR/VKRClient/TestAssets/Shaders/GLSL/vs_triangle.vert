#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

//vec2 positions[6] = vec2[]
//(
//    vec2(-1.0, -1.0),
//    vec2(1.0, -1.0),
//    vec2(1.0, 1.0),
//    vec2(1.0, 1.0),
//    vec2(-1.0, 1.0),
//    vec2(-1.0, -1.0)
//);

//vec4 colors[6] = vec4[]
//(
//    vec4(1.0f, 0.0f, 0.0f, 1.0f),
//    vec4(0.0f, 1.0f, 0.0f, 1.0f),
//    vec4(0.0f, 0.0f, 1.0f, 1.0f),
//    vec4(0.0f, 0.0f, 1.0f, 1.0f),
//    vec4(0.0f, 1.0f, 0.0f, 1.0f),
//    vec4(1.0f, 0.0f, 0.0f, 1.0f)
//);

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec4 inTexCoord;

struct VS_OUT
{
    vec4 vtxColor;
};

layout (location = 0) out VS_OUT vsOutput;

layout(set = 0, binding = 0) uniform BindingIndices
{
    int mvpIndex;
} bindingIndices;

layout(set = 2, binding = 0) uniform MVP
{
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp[];

void main() 
{
    mat4 model = mvp[nonuniformEXT(bindingIndices.mvpIndex)].model;
    mat4 view = mvp[nonuniformEXT(bindingIndices.mvpIndex)].view;
    mat4 proj = mvp[nonuniformEXT(bindingIndices.mvpIndex)].proj;

    gl_Position = proj * view * model * inPosition;
    vsOutput.vtxColor = inPosition;
}