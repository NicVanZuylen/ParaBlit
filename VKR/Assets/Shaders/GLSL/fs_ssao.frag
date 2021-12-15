#version 450
#include "Common/pb_common.h"
#include "Common/view_constants.h"
#extension GL_EXT_samplerless_texture_functions : require

#define AO_KERNEL_SIZE 32

struct FS_IN
{
    vec2 texCoord;
    vec2 pad0;
};

layout(push_constant) uniform Bindings
{
    uint viewConstantsIndex;
    uint aoConstantsIndex;
    uint randomRotationTexIndex;
    uint depthIndex;
    uint normalIndex;
    uint srcSamplerIndex;
    uint randomRotationSamplerIdx;
} PB_BINDINGS_NAME;

DEFINE_VIEW_CONSTANTS(viewConstants)

layout(set = 1, binding = 0) uniform AOConstants
{
    float sampleRadius;
    float depthBias;
    float depthSlopeBias; // Represents extra bias added at acute angles relative to the camera where extra bias may be needed.
    float depthSlopeThreshold;
    float intensity;
    uint renderWidth;
    uint renderHeight;
    float pad0;
    vec4 samples[AO_KERNEL_SIZE];
} aoConstants[];

PB_DEFINE_TEXTURE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

#define AO_CONST PB_UBO(aoConstants, aoConstantsIndex)
#define VIEW_CONST PB_UBO(viewConstants, viewConstantsIndex)

layout(location = 0) in FS_IN fsInput;

layout(location = 0) out vec4 outColor;

void main()
{
    ivec2 rotationTileSize = textureSize(PB_TEXTURE(randomRotationTexIndex), 0);
    ivec2 dstSize = ivec2(AO_CONST.renderWidth, AO_CONST.renderHeight);

    vec2 randomRotationCoordScale = vec2(dstSize / rotationTileSize);
    vec2 randomRotationCoord = fsInput.texCoord * randomRotationCoordScale;

    float depth = texture(PB_BUILD_SAMPLER(depthIndex, srcSamplerIndex), fsInput.texCoord).r;

    vec3 worldNormal = texture(PB_BUILD_SAMPLER(normalIndex, srcSamplerIndex), fsInput.texCoord).xyz;
    // SSAO calculations operate in view space, so convert the normal from world to view space.
    vec3 normal = (VIEW_CONST.view * vec4(worldNormal.xyz, 0.0)).xyz;

    vec3 viewPos = ReconstructPositionFromDepth((fsInput.texCoord * 2.0) - 1.0, depth, VIEW_CONST.aspectRatio, VIEW_CONST.tanHalfFOV, VIEW_CONST.proj);

    vec3 randRotationVector = texture(PB_BUILD_SAMPLER(randomRotationTexIndex, randomRotationSamplerIdx), randomRotationCoord).xyz;
    vec3 tangent = normalize(randRotationVector - normal * dot(randRotationVector, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float radius = AO_CONST.sampleRadius;
    float bias = AO_CONST.depthBias * (1.0 - viewPos.z); // Bias needs to vary based on Z. Since the view Z is linear, this expression does the trick.
    float slope = 1.0 - abs(dot(normal, worldNormal));
    if(slope <= AO_CONST.depthSlopeThreshold)
        bias += mix(0.0, AO_CONST.depthSlopeBias, slope / AO_CONST.depthSlopeThreshold);

    float ao = float(AO_KERNEL_SIZE);

    for(int i = 0; i < AO_KERNEL_SIZE; ++i)
    {
        vec3 samplePos = TBN * AO_CONST.samples[i].xyz * radius;
        samplePos += viewPos;

        vec4 offset = vec4(samplePos, 1.0);
        offset = VIEW_CONST.proj * offset;
        offset.xy /= offset.w;
        offset.xy = (offset.xy * 0.5) + 0.5;

        float depthSample = texture(PB_BUILD_SAMPLER(depthIndex, srcSamplerIndex), offset.xy).r;
        depthSample = ReconstructViewZFromDepth(depthSample, VIEW_CONST.proj);
        depthSample -= bias;

        if(abs(depthSample - samplePos.z) <= radius)
        {
            ao -= step(samplePos.z + bias, depthSample);
        }
    }
    ao /= AO_KERNEL_SIZE;
    ao = pow(ao, AO_CONST.intensity);

    outColor = vec4(ao, ao, ao, 1.0);
}