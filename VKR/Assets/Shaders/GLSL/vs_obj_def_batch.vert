#version 450
#include "Common/pb_common.h"
#include "Common/view_constants.h"
#include "Common/vertex_common.h"

#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Bindings
{
    uint viewConstantsIndex;
    uint instanceBufferIndex;
} PB_BINDINGS_NAME;

DEFINE_VIEW_CONSTANTS(viewConstants);
#define VIEW_CONST PB_UBO(viewConstants, viewConstantsIndex)

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

layout(location = 0) out VS_OUT
{
    vec4 position;
    vec2 texCoord;

    // This is a draw batch shader, so textures and sampler may differ on a per-object/per-vertex basis.
    flat uint samplerIdx;
    flat uint vertexIndex;
    flat uint textureIndices[INSTANCE_TEXTURE_COUNT];

    mat3 tbnMatrix; // Normal is 3rd vector component.
} vsOutput;

void main() 
{
    uint vertexIndex = uint(gl_VertexIndex) & 0xFFFFFF; // Mask out final 8 bits for vertex index.
    uint instanceIndex = uint(gl_VertexIndex) >> 24; // Shift 24 bits right for instance index.

    VS_INSTANCE vsInstance = instanceBuffers[nonuniformEXT(PB_BINDINGS_NAME.instanceBufferIndex)].instances[nonuniformEXT(instanceIndex)];
    VS_IN vsInput = vertexBuffers[nonuniformEXT(vsInstance.vertexIndex)].vertices[nonuniformEXT(vertexIndex)];

    vec4 position;
    vec2 texCoord;
    vec3 normal;
    vec3 tangent;
    UnpackVertexAttibutes(vsInput, position, texCoord, normal, tangent);

    mat3 modelCpy = mat3(vsInstance.model);
    vec3 biTangent = -cross(normal, tangent);

    vsOutput.tbnMatrix = mat3
    (
        modelCpy * tangent,     // t
        modelCpy * biTangent,   // b
        modelCpy * normal       // n
    );

    gl_Position = VIEW_CONST.viewProj * vsInstance.model * position;
    vsOutput.position = vsInstance.model * position;
    vsOutput.position.w = 1.0;
    vsOutput.texCoord = texCoord;

    vsOutput.samplerIdx = vsInstance.samplerIndex;
    for(uint i = 0; i < INSTANCE_TEXTURE_COUNT; ++i)
        vsOutput.textureIndices[i] = vsInstance.textureIndices[i];
}