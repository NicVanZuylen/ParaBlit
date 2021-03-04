#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(push_constant) uniform Bindings
{
    uint mvpIndex;
    uint vertexBufferIndex;
    uint instanceBufferIndex;
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

layout(location = 0) out VS_OUT
{
    vec4 position;
    vec2 texCoord;

    // This is a draw batch shader, so textures and sampler may differ on a per-object/per-vertex basis.
    flat uint samplerIdx;
    flat uint textureIndices[7];

    mat3 tbnMatrix; // Normal is 3rd vector component.
} vsOutput;

void main() 
{
    uint vertexIndex = uint(gl_VertexIndex) & 0xFFFFFF; // Mask out final 8 bits for vertex index.
    uint instanceIndex = uint(gl_VertexIndex) >> 24; // Shift 24 bits right for instance index.

    VS_IN vsInput = vertexBuffers[nonuniformEXT(bindings.vertexBufferIndex)].vertices[nonuniformEXT(vertexIndex)];
    VS_INSTANCE vsInstance = instanceBuffers[nonuniformEXT(bindings.instanceBufferIndex)].instances[nonuniformEXT(instanceIndex)];

    mat3 modelCpy = mat3(vsInstance.model);
    vec3 biTangent = cross(vsInput.normal.xyz, vsInput.tangent.xyz);

    vsOutput.tbnMatrix = mat3
    (
        modelCpy * vsInput.tangent.xyz,  // t
        modelCpy * biTangent,            // b
        modelCpy * vsInput.normal.xyz    // n
    );

    gl_Position = mvp[nonuniformEXT(bindings.mvpIndex)].proj * mvp[nonuniformEXT(bindings.mvpIndex)].view * vsInstance.model * vsInput.position;
    vsOutput.position = vsInstance.model * vsInput.position;
    vsOutput.position.w = 1.0;
    vsOutput.texCoord = vsInput.texCoord;

    vsOutput.samplerIdx = vsInstance.samplerIndex;
    for(uint i = 0; i < INSTANCE_TEXTURE_COUNT; ++i)
        vsOutput.textureIndices[i] = vsInstance.textureIndices[i];
}