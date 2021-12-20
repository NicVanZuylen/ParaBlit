#version 450
#include "Common/pb_common.h"
#extension GL_ARB_separate_shader_objects : enable

// Resources are accessed via index, and each input from the vertex shader may contain a unique set of textures and sampler.
#extension GL_EXT_nonuniform_qualifier : enable

layout(push_constant) uniform Bindings
{
    uint mvpIndex;
} PB_BINDINGS_NAME;

PB_DEFINE_TEXTURE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

layout (location = 0) in FS_IN
{
    vec4 position;
    vec2 texCoord;
    flat uint samplerIdx;
    flat uint vertexIdx;
    flat uint textureIndices[6];
    mat3 tbnMatrix;
} fsInput;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outSpecAndRough;

void main() 
{
    uint samplerIdx = fsInput.samplerIdx;
    uint colorIdx = fsInput.textureIndices[0];
    uint normalIdx = fsInput.textureIndices[1];
    uint specIdx = fsInput.textureIndices[2];
    uint roughIdx = fsInput.textureIndices[3];
    uint emissionIdx = fsInput.textureIndices[4];

    vec4 color = texture
    (
        sampler2D(PB_TEXTURE_BINDINGS_NAME[nonuniformEXT(colorIdx)], PB_SAMPLER_BINDINGS_NAME[nonuniformEXT(samplerIdx)]), 
        fsInput.texCoord
    );

    vec3 normal = texture
    (
        sampler2D(PB_TEXTURE_BINDINGS_NAME[nonuniformEXT(normalIdx)], PB_SAMPLER_BINDINGS_NAME[nonuniformEXT(samplerIdx)]), 
        fsInput.texCoord
    ).xyz;

    vec3 spec = texture
    (
        sampler2D(PB_TEXTURE_BINDINGS_NAME[nonuniformEXT(specIdx)], PB_SAMPLER_BINDINGS_NAME[nonuniformEXT(samplerIdx)]), 
        fsInput.texCoord
    ).rgb;

    float roughness = texture
    (
        sampler2D(PB_TEXTURE_BINDINGS_NAME[nonuniformEXT(roughIdx)], PB_SAMPLER_BINDINGS_NAME[nonuniformEXT(samplerIdx)]), 
        fsInput.texCoord
    ).r;

    vec3 emission = texture
    (
        sampler2D(PB_TEXTURE_BINDINGS_NAME[nonuniformEXT(emissionIdx)], PB_SAMPLER_BINDINGS_NAME[nonuniformEXT(samplerIdx)]), 
        fsInput.texCoord
    ).rgb;

    outColor = (emission.r + emission.g + emission.b == 0.0) ? vec4(color.rgb, 0.0) : vec4(emission.rgb, 1.0); // Alpha is used as emission mask.
    outNormal = vec4(normalize(fsInput.tbnMatrix * (normal * 2.0 - 1.0)), 1.0);
    outSpecAndRough = vec4(spec, roughness);
}