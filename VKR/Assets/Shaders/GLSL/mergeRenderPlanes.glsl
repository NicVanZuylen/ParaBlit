#version 460
#extension GL_EXT_samplerless_texture_functions : require

#include "Common/pb_common.h"

#define PERMUTATION_ShaderStage 2 // 0 == Vertex, 1 == Fragment
#define PERMUTATION_ChannelCount 4
#define PERMUTATION_FillColor 2 // 0 == Black, 1 == White

layout(push_constant) uniform Bindings
{
    uint mergeConstantsIndex;
    uint srcTextureIndex;
    uint srcSamplerIndex;
} PB_BINDINGS_NAME;

struct VS_OUT
{
    vec2 texCoord;
    vec2 pad0;
};

layout(set = 1, binding = 0) uniform MergeConstants
{
    uvec2 srcOffset;
    uvec2 srcDim;
} mergeConstants[];
#define MERGE_CONST PB_UBO(mergeConstants, mergeConstantsIndex)

// ********************************************************************************************************************************
// Vertex
#if PERMUTATION_ShaderStage == 0
// ********************************************************************************************************************************

vec2 positions[6] = vec2[]
(
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 1.0),
    vec2(-1.0, 1.0),
    vec2(-1.0, -1.0)
);

layout (location = 0) out VS_OUT vsOutput;

void main() 
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.01, 1.0);
    vsOutput.texCoord = (positions[gl_VertexIndex] + 1) * 0.5;
}

// ********************************************************************************************************************************
#endif
// ********************************************************************************************************************************

// ********************************************************************************************************************************
// Fragment
#if PERMUTATION_ShaderStage == 1
// ********************************************************************************************************************************
PB_DEFINE_TEXTURE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

layout(location = 0) in VS_OUT fsInput;

layout(location = 0) out vec4 outColor;

vec4 SrcSample()
{
    const float fillColor = (PERMUTATION_FillColor == 0) ? 0.0 : 1.0;

    uvec2 srcSize = textureSize(PB_TEXTURE(srcTextureIndex), 0);
    vec2 srcCoordMin = vec2(MERGE_CONST.srcOffset) / srcSize;
    vec2 srcCoordDim = vec2(MERGE_CONST.srcDim) / srcSize;
    vec2 srcCoordMax = srcCoordMin + srcCoordDim;

    vec2 mappedCoord = srcCoordMin + (fsInput.texCoord * srcCoordDim);
    mappedCoord = clamp(mappedCoord, srcCoordMin, srcCoordMax);

#if PERMUTATION_ChannelCount == 0
    float colorSample = texture(PB_BUILD_SAMPLER(srcTextureIndex, srcSamplerIndex), mappedCoord).r;
    return vec4(colorSample, fillColor, fillColor, fillColor);
#elif PERMUTATION_ChannelCount == 1
    vec2 colorSample = texture(PB_BUILD_SAMPLER(srcTextureIndex, srcSamplerIndex), mappedCoord).rg;
    return vec4(colorSample, fillColor, fillColor);
#elif PERMUTATION_ChannelCount == 2
    vec3 colorSample = texture(PB_BUILD_SAMPLER(srcTextureIndex, srcSamplerIndex), mappedCoord).rgb;
    return vec4(colorSample, fillColor);
#elif PERMUTATION_ChannelCount == 3
    vec4 colorSample = texture(PB_BUILD_SAMPLER(srcTextureIndex, srcSamplerIndex), mappedCoord).rgba;
    return colorSample;
#endif
}

void main()
{
    outColor = SrcSample();
}

// ********************************************************************************************************************************
#endif
// ********************************************************************************************************************************