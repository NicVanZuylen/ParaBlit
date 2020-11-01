#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in mat4 model;

struct VS_OUT
{
    vec4 normal;
    vec4 position;
    vec2 texCoord;
    vec2 pad0;
    mat3 tbnMatrix;
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
    mat4 invView;
    mat4 invProj;
    vec4 cameraPosition;
} mvp[];

layout (location = 0) out VS_OUT vsOutput;

void main() 
{
    mat4 view = mvp[nonuniformEXT(bindings.mvpIndex)].view;
    mat4 proj = mvp[nonuniformEXT(bindings.mvpIndex)].proj;

    mat3 modelCpy = mat3(model[0].xyz, model[1].xyz, model[2].xyz);

    vec3 biTangent = normalize(cross(inNormal.xyz, inTangent.xyz));

    vec3 t = modelCpy * inTangent.xyz;
	vec3 b = modelCpy * biTangent;
	vec3 n = modelCpy * inNormal.xyz;

    vsOutput.tbnMatrix = mat3(t, b, n);

    gl_Position = proj * view * model * inPosition;
    vsOutput.normal = normalize(model * vec4(inNormal.xyz, 0.0));
    vsOutput.normal.w = 1.0;
    vsOutput.position = model * inPosition;
    vsOutput.position.w = 1.0;
    vsOutput.texCoord = inTexCoord;
}