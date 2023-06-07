#version 450
#include "Common/pb_common.h"
#include "Common/drawbatch_common.h"
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
    vec2 texCoord;

    mat3 tbnMatrix;
} fsInput;

layout (location = 5) perprimitiveEXT in PRIMITIVE_IN
{
    flat uint meshletIndex;
    flat uint meshletCount;
    flat uint samplerIdx;
    flat uint vertexIdx;
    flat uint textureIndices[6];
} fsInputPrimitive;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outSpecAndRough;

float rand(float co) { return fract(sin(co*(91.3458)) * 47453.5453); }

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

    const vec3 gamma = vec3(1.0 / 2.2);

    float meshletPos = float(fsInputPrimitive.meshletIndex) / fsInputPrimitive.meshletCount;

    outColor = (emission.r + emission.g + emission.b == 0.0) ? vec4(color.rgb, 0.0) : vec4(emission.rgb, 1.0); // Alpha is used as emission mask.
    outColor.rgb = pow(outColor.rgb, gamma);

    //outColor.r = rand(meshletPos);
    //outColor.g = rand(outColor.r);
    //outColor.b = rand(outColor.g);
    //outColor.a = 1.0;

    outNormal = vec4(normalize(fsInput.tbnMatrix * (normal * 2.0 - 1.0)), 1.0);

    outSpecAndRough = vec4(pow(spec, gamma), roughness);
}