#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(push_constant) uniform Bindings
{
    uint svbIndex;
    uint vertexBufferIndex;
    uint instanceBufferIndex;
} bindings;

layout(set = 1, binding = 0) uniform ShadowViewBuffer
{
    mat4 view;
    mat4 proj;
} svb[];

struct VS_IN
{
    vec4 position;
    vec4 normal;
    vec4 tangent;
    vec2 texCoord;
    vec2 pad0;
};

layout(set = 0, binding = 2) buffer VertexBuffer
{
    VS_IN vertices[];
} vertexBuffers[];

#define INSTANCE_TEXTURE_COUNT 7

struct VS_INSTANCE
{
    mat4 model;
    uint textureIndices[INSTANCE_TEXTURE_COUNT];
    uint samplerIndex;
};

layout(set = 0, binding = 2) buffer InstanceBuffer
{
    VS_INSTANCE instances[];
} instanceBuffers[];

void main() 
{
    uint vertexIndex = uint(gl_VertexIndex) & 0xFFFFFF; // Mask out final 8 bits for vertex index.
    uint instanceIndex = uint(gl_VertexIndex) >> 24; // Shift 24 bits right for instance index.

    VS_IN vsInput = vertexBuffers[nonuniformEXT(bindings.vertexBufferIndex)].vertices[nonuniformEXT(vertexIndex)];
    VS_INSTANCE vsInstance = instanceBuffers[nonuniformEXT(bindings.instanceBufferIndex)].instances[nonuniformEXT(instanceIndex)];

    gl_Position = svb[nonuniformEXT(bindings.svbIndex)].proj * svb[nonuniformEXT(bindings.svbIndex)].view * vsInstance.model * vsInput.position;
}