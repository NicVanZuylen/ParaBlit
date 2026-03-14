#version 450
#include "Common/pb_common.h"
#include "Common/drawbatch_common.h"
#include "Common/gbuffer_common.h"
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_mesh_shader : require

layout(push_constant) uniform Bindings
{
    DEFINE_REQUIRED_UNIFORM_BINDINGS;
} PB_BINDINGS_NAME;

PB_DEFINE_TEXTURE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

layout (location = 0) in VERTEX_IN
{
    vec4 position;
    vec4 positionLastFrame;
    vec2 texCoord;

    mat3 tbnMatrix;
} fsInput;

layout (location = 6) perprimitiveEXT in PRIMITIVE_IN
{
    flat uint samplerIdx;
    flat uint vertexIdx;
    flat uint textureIndices[6];
} fsInputPrimitive;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outSpecAndRough;
layout(location = 3) out vec2 outMotionVectors;

void main() 
{
    uint samplerIdx = fsInputPrimitive.samplerIdx;
    uint colorIdx = fsInputPrimitive.textureIndices[0];
    uint normalIdx = fsInputPrimitive.textureIndices[1];
    uint specIdx = fsInputPrimitive.textureIndices[2];
    uint roughIdx = fsInputPrimitive.textureIndices[3];
    uint emissionIdx = fsInputPrimitive.textureIndices[4];

    vec4 color = texture
    (
        sampler2D(PB_TEXTURE_NU(colorIdx), PB_SAMPLER_NU(samplerIdx)), 
        fsInput.texCoord
    );

    vec3 normal = texture
    (
        sampler2D(PB_TEXTURE_NU(normalIdx), PB_SAMPLER_NU(samplerIdx)), 
        fsInput.texCoord
    ).xyz;

    vec3 spec = texture
    (
        sampler2D(PB_TEXTURE_NU(specIdx), PB_SAMPLER_NU(samplerIdx)), 
        fsInput.texCoord
    ).rgb;

    float roughness = texture
    (
        sampler2D(PB_TEXTURE_NU(roughIdx), PB_SAMPLER_NU(samplerIdx)), 
        fsInput.texCoord
    ).r;

    vec3 emission = texture
    (
        sampler2D(PB_TEXTURE_NU(emissionIdx), PB_SAMPLER_NU(samplerIdx)), 
        fsInput.texCoord
    ).rgb;

    const vec3 gamma = vec3(1.0 / 2.2);

    if (emission.r + emission.g + emission.b == 0.0)
    {
        outColor = vec4(color.rgb, 0.0);
        outColor.rgb = pow(outColor.rgb, gamma);
    }
    else
    {
        vec3 emissionEncoded = EncodeColorF(emission.rgb, 10);
        outColor = vec4(emissionEncoded, 1.0);
    }

    outNormal = vec4(normalize(fsInput.tbnMatrix * (normal * 2.0 - 1.0)), 1.0);
    PackNormal(outNormal.xyz);

    outSpecAndRough = vec4(pow(spec, gamma), roughness);

    // Calculate motion vectors..
    {
        vec2 clipPos = fsInput.position.xy / fsInput.position.w * 0.5 + 0.5;
        vec2 clipPosLastFrame = fsInput.positionLastFrame.xy / fsInput.positionLastFrame.w * 0.5 + 0.5;

        outMotionVectors = (clipPos - clipPosLastFrame);
    }
}