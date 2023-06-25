#version 450
#include "Common/pb_common.h"
#include "Common/view_constants.h"
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_samplerless_texture_functions : require

struct FS_IN
{
    vec2 texCoord;
    vec2 pad0;
};

#define SHADOW_CASCADE_COUNT 4

layout(push_constant) uniform Bindings
{
    uint mvpIndex;
    uint shadowCascadeViewIndices[SHADOW_CASCADE_COUNT];
    uint gDepthIndex;
    uint gNormalIndex;
    uint gBufferSamplerIdx;
    uint shadowmapIndex;
    uint shadowSamplerIdx;
    uint randomRotationTextureIndex;
    uint rotationSamplerIdx;
} PB_BINDINGS_NAME;

DEFINE_VIEW_CONSTANTS(mvp);

layout(set = 1, binding = 0) uniform ShadowConstants
{
    mat4 viewProj;
    vec4 shadowViewDirection;
    vec2 cascadeRange;
    float shadowPenumbraDistance;
    float shadowBiasMultiplier;
} shadowConstants[];

PB_DEFINE_TEXTURE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;
layout(set = 0, binding = 0) uniform texture2DArray texturesArray[];

layout(location = 0) in FS_IN fsInput;

layout(location = 0) out vec4 outColor;

#define MVP_CONST PB_UBO(mvp, mvpIndex)
#define SHAD_CONST(cascade) PB_UBO(shadowConstants, shadowCascadeViewIndices[cascade])

vec3 ScreenPosFromWorld(vec3 worldPosition, in mat4 viewProj)
{
    vec4 screenPosRaw = viewProj * vec4(worldPosition, 1.0);
    return screenPosRaw.xyz / screenPosRaw.w;
}

#define SHADOWMAP_SAMPLER sampler2DArray(texturesArray[PB_BINDINGS_NAME.shadowmapIndex], PB_SAMPLER(shadowSamplerIdx))

float SampleShadowmapDepth(vec2 sampleCoord, uint cascade)
{
    return texture
    (
        SHADOWMAP_SAMPLER,
        vec3(sampleCoord, float(cascade))
    ).r;
}

vec4 SampleShadowmap4(vec2 sampleCoord, float refDepth, uint cascade)
{
    vec4 depths = textureGather
    (
        SHADOWMAP_SAMPLER,
        vec3(sampleCoord, float(cascade))
    );
    depths.x = (refDepth > depths.x) ? 0.0 : 1.0;
    depths.y = (refDepth > depths.y) ? 0.0 : 1.0;
    depths.z = (refDepth > depths.z) ? 0.0 : 1.0;
    depths.w = (refDepth > depths.w) ? 0.0 : 1.0;
    return depths;
}

float BilinearShadow(vec3 shadowmapScreenPos, vec2 texelSize, float shadowBias, uint cascade)
{
    vec2 sampleCoord = (shadowmapScreenPos.xy * 0.5) + 0.5;
    vec2 fractPart = mod(sampleCoord, texelSize) / texelSize;

    vec4 samples = SampleShadowmap4(sampleCoord - (texelSize * 0.502), shadowmapScreenPos.z - shadowBias, cascade);
    float blTexel = samples.w;
    float brTexel = samples.z;
    float tlTexel = samples.x;
    float trTexel = samples.y;

    float mixA = mix(blTexel, tlTexel, fractPart.y);
    float mixB = mix(brTexel, trTexel, fractPart.y);

    return mix(mixA, mixB, fractPart.x);
}

#define BLOCKER_SEARCH_SAMPLE_COUNT 32
#define DISK_PCF_SAMPLE_COUNT 32
#define DISK_PCF_INV_SAMPLE_COUNT (1.0 / DISK_PCF_SAMPLE_COUNT)

const vec2 poissonSampleOffsets[] = vec2[]
(
	vec2(0.06407013, 0.05409927),
	vec2(0.7366577, 0.5789394),
	vec2(-0.6270542, -0.5320278),
	vec2(-0.4096107, 0.8411095),
	vec2(0.6849564, -0.4990818),
	vec2(-0.874181, -0.04579735),
	vec2(0.9989998, 0.0009880066),
	vec2(-0.004920578, -0.9151649),
	vec2(0.1805763, 0.9747483),
	vec2(-0.2138451, 0.2635818),
	vec2(0.109845, 0.3884785),
	vec2(0.06876755, -0.3581074),
	vec2(0.374073, -0.7661266),
	vec2(0.3079132, -0.1216763),
	vec2(-0.3794335, -0.8271583),
	vec2(-0.203878, -0.07715034),
	vec2(0.5912697, 0.1469799),
	vec2(-0.88069, 0.3031784),
	vec2(0.5040108, 0.8283722),
	vec2(-0.5844124, 0.5494877),
	vec2(0.6017799, -0.1726654),
	vec2(-0.5554981, 0.1559997),
	vec2(-0.3016369, -0.3900928),
	vec2(-0.5550632, -0.1723762),
	vec2(0.925029, 0.2995041),
	vec2(-0.2473137, 0.5538505),
	vec2(0.9183037, -0.2862392),
	vec2(0.2469421, 0.6718712),
	vec2(0.3916397, -0.4328209),
	vec2(-0.03576927, -0.6220032),
	vec2(-0.04661255, 0.7995201),
	vec2(0.4402924, 0.3640312)
);

vec2 PCFDiskBlocker(vec3 shadowmapScreenPos, vec2 texelSize, vec2 diskRotation, float shadowBias, float refPenumbraDistance, uint cascade)
{
    float blockerAccum = 0.0;
    float numValidBlockerSamples = 0.0;

    for(int i = 0; i < BLOCKER_SEARCH_SAMPLE_COUNT; ++i)
    {
        vec2 diskSample = poissonSampleOffsets[i] * diskRotation * (texelSize * refPenumbraDistance);
        float depth = SampleShadowmapDepth(((shadowmapScreenPos.xy * 0.5) + 0.5) + (diskSample * 0.5), cascade) + shadowBias;
        float receiverDepth = shadowmapScreenPos.z;

        if(depth < receiverDepth)
        {
            blockerAccum += depth;
            numValidBlockerSamples += 1.0;
        }
    }

    return vec2(blockerAccum, numValidBlockerSamples);
}

float PCFDiskShadow(vec3 shadowmapScreenPos, vec2 texelSize, vec2 diskRotation, float shadowBias, float penumbraDistance, float refPenumbraDistance, uint cascade)
{
    float shadowStrength = 0.0;

    for(int i = 0; i < DISK_PCF_SAMPLE_COUNT; ++i)
    {
        vec2 diskSample = poissonSampleOffsets[i] * diskRotation * texelSize * penumbraDistance;
        shadowStrength += BilinearShadow(shadowmapScreenPos + vec3(diskSample, 0.0), texelSize, shadowBias, cascade);
    }

    return shadowStrength * DISK_PCF_INV_SAMPLE_COUNT;
}

float ShadowPCSS(vec3 shadowmapScreenPos, uint cascade, float shadowBias, float refPenumbraDistance, out float penumbraDistance)
{
    vec2 textureDimensions = textureSize(PB_TEXTURE(shadowmapIndex), 0);
    vec2 texelSize = 1.0 / textureDimensions;

    vec2 randomRotation = texture
    (
        PB_BUILD_SAMPLER(randomRotationTextureIndex, rotationSamplerIdx),
        shadowmapScreenPos.xy * textureDimensions
    ).rg;

    vec2 blockerResults = PCFDiskBlocker(shadowmapScreenPos, texelSize, randomRotation, shadowBias, refPenumbraDistance, cascade);
    if(blockerResults.y == 0.0)
        return 1.0;

    float blockerAverage = blockerResults.x / blockerResults.y;
    penumbraDistance = ((shadowmapScreenPos.z - blockerAverage) * 10 * refPenumbraDistance) / blockerAverage;

    return PCFDiskShadow(shadowmapScreenPos, texelSize, randomRotation, shadowBias, penumbraDistance, refPenumbraDistance, cascade);
}

void main()
{
    float depth = texture(PB_BUILD_SAMPLER(gDepthIndex, gBufferSamplerIdx), fsInput.texCoord).r;
    vec3 normal = texture(PB_BUILD_SAMPLER(gNormalIndex, gBufferSamplerIdx), fsInput.texCoord).rgb;
    vec3 position = WorldPosFromDepth(depth, fsInput.texCoord, MVP_CONST.invView, MVP_CONST.invProj);

    uint cascadeIdx = ~uint(0);
    float viewZ = -ReconstructViewZFromDepth(depth, MVP_CONST.proj) * 2.0;
    for(uint i = 0; i < SHADOW_CASCADE_COUNT; ++i)
    {
        vec2 range = SHAD_CONST(i).cascadeRange;
        if(viewZ < range[1])
        {
            cascadeIdx = i;
            break;
        }
    }

    float normalDotLight = dot(normal, SHAD_CONST(cascadeIdx).shadowViewDirection.xyz);
    vec3 shadowmapScreenPos = ScreenPosFromWorld(position, SHAD_CONST(cascadeIdx).viewProj);

    float biasMult = SHAD_CONST(cascadeIdx).shadowBiasMultiplier;
    float shadowBias = max((0.003 * biasMult) * (1.0 - normalDotLight), (0.0015 * biasMult));

    float refShadowPenumbraDistance = SHAD_CONST(cascadeIdx).shadowPenumbraDistance;
    float shadowPenumbraDistance = 0.01;
    float shadowMask = ShadowPCSS(shadowmapScreenPos, cascadeIdx, shadowBias, refShadowPenumbraDistance, shadowPenumbraDistance);

    outColor = vec4(cascadeIdx < SHADOW_CASCADE_COUNT ? shadowMask : 1.0);
}