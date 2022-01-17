#version 450
#include "Common/pb_common.h"
#include "Common/vertex_common.h"

#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Bindings
{
    uint svbIndex;
    uint instanceBufferIndex;
} bindings;

layout(set = 1, binding = 0) uniform ShadowViewBuffer
{
    mat4 view;
    mat4 proj;
} svb[];

layout(std430, set = 0, binding = 2) readonly buffer VertexBuffer
{
    VS_IN vertices[];
} vertexBuffers[];

#define INSTANCE_TEXTURE_COUNT 6

struct VS_INSTANCE
{
    mat4 model;
    uint textureIndices[INSTANCE_TEXTURE_COUNT];
    uint vertexIndex;
    uint samplerIndex;
};

layout(set = 0, binding = 2) readonly buffer InstanceBuffer
{
    VS_INSTANCE instances[];
} instanceBuffers[];

void main() 
{
    uint vertexIndex = uint(gl_VertexIndex) & 0xFFFFFF; // Mask out final 8 bits for vertex index.
    uint instanceIndex = uint(gl_VertexIndex) >> 24; // Shift 24 bits right for instance index.

    VS_INSTANCE vsInstance = instanceBuffers[nonuniformEXT(bindings.instanceBufferIndex)].instances[nonuniformEXT(instanceIndex)];
    VS_IN vsInput = vertexBuffers[nonuniformEXT(vsInstance.vertexIndex)].vertices[nonuniformEXT(vertexIndex)];

    gl_Position = svb[nonuniformEXT(bindings.svbIndex)].proj * svb[nonuniformEXT(bindings.svbIndex)].view * vsInstance.model * vec4(vsInput.position, 1.0);
}