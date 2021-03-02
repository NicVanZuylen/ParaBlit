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
    vec4 normal;
    vec4 position;
    vec2 texCoord;

    // This is a draw batch shader, so textures and sampler may differ on a per-object/per-vertex basis.
    flat uint samplerIdx;
    flat uint textureIndices[7];

    mat3 tbnMatrix;
} vsOutput;

void main() 
{
    uint vertexIndex = uint(gl_VertexIndex) & 0xFFFFFF; // Mask out final 8 bits for vertex index.
    uint instanceIndex = uint(gl_VertexIndex) >> 24; // Shift 24 bits right for instance index.

    VS_IN vsInput = vertexBuffers[nonuniformEXT(bindings.vertexBufferIndex)].vertices[nonuniformEXT(vertexIndex)];
    VS_INSTANCE vsInstance = instanceBuffers[nonuniformEXT(bindings.instanceBufferIndex)].instances[nonuniformEXT(instanceIndex)];

    mat4 model = mvp[nonuniformEXT(bindings.mvpIndex)].model;
    mat4 view = mvp[nonuniformEXT(bindings.mvpIndex)].view;
    mat4 proj = mvp[nonuniformEXT(bindings.mvpIndex)].proj;

    mat3 modelCpy = mat3(vsInstance.model[0].xyz, vsInstance.model[1].xyz, vsInstance.model[2].xyz);

    vec3 biTangent = normalize(cross(vsInput.normal.xyz, vsInput.tangent.xyz));

    vec3 t = modelCpy * vsInput.tangent.xyz;
	vec3 b = modelCpy * biTangent;
	vec3 n = modelCpy * vsInput.normal.xyz;

    vsOutput.tbnMatrix = mat3(t, b, n);

    gl_Position = proj * view * vsInstance.model * vsInput.position;
    vsOutput.normal = normalize(vsInstance.model * vec4(vsInput.normal.xyz, 0.0));
    vsOutput.normal.w = 1.0;
    vsOutput.position = vsInstance.model * vsInput.position;
    vsOutput.position.w = 1.0;
    vsOutput.texCoord = vsInput.texCoord;

    vsOutput.samplerIdx = vsInstance.samplerIndex;
    for(uint i = 0; i < INSTANCE_TEXTURE_COUNT; ++i)
        vsOutput.textureIndices[i] = vsInstance.textureIndices[i];
}