#version 450
#include "Common/pb_common.h"
#include "Common/ibl_convolution_common.h"
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Bindings
{
    uint cubeConstantsIndex;
    uint materialConstantsIndex;
    uint srcCubeIndex;
    uint srcSamplerIndex;
} PB_BINDINGS_NAME;

PB_DEFINE_TEXTURE_CUBE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

layout(location = 0) in vec3 pos;

layout(location = 0) out vec4 outColor;

#define SRC_SAMPLER PB_BUILD_SAMPLER_CUBE(srcCubeIndex, srcSamplerIndex)

const vec3 AxisCorrection = vec3(1.0, -1.0, -1.0);

layout(set = 1, binding = 0) uniform MaterialConstantsLayout
{
    float roughness;
    vec3 pad;
} constants[];

#define CONSTANTS PB_UBO(constants, materialConstantsIndex)

const uint SampleCount = 4096;
void main()
{
    vec3 prefilteredColor = vec3(0.0);

    vec3 N = normalize(pos) * AxisCorrection;
    vec3 R = N;
    vec3 V = R;

    float roughness = CONSTANTS.roughness;
    float totalWeight = 0.0;
    for(uint i = 0; i < SampleCount; ++i)
    {
        vec2 Xi = Hammersley(i, SampleCount);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float nDotL = max(dot(N, L), 0.0);
        if(nDotL > 0.0)
        {
            prefilteredColor += texture(SRC_SAMPLER, L).rgb * nDotL;
            totalWeight += nDotL;
        }
    }
    prefilteredColor /= totalWeight;

    outColor = vec4(prefilteredColor, 1.0);
}