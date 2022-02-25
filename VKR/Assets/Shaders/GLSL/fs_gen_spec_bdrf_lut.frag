#version 450
#include "Common/pb_common.h"
#include "Common/ibl_convolution_common.h"
#include "Common/pbr.h"
#extension GL_ARB_separate_shader_objects : enable

struct FS_IN
{
    vec2 texCoord;
    vec2 pad0;
};

layout(location = 0) in FS_IN fsInput;
layout(location = 0) out vec4 outColor;

const vec3 AxisCorrection = vec3(1.0, -1.0, -1.0);

layout(set = 1, binding = 0) uniform MaterialConstantsLayout
{
    float roughness;
    vec3 pad;
} constants[];

const uint SampleCount = 1024;
void main()
{
    float nDotV = fsInput.texCoord.x;
    float roughness = fsInput.texCoord.y;

    float scale = 0.0;
    float bias = 0.0;

    vec3 V = vec3
    (
        sqrt(1.0 - nDotV * nDotV),
        0.0,
        nDotV
    );

    vec3 N = vec3(0.0, 0.0, 1.0);

    float k = (roughness * roughness) / 2.0;
    for(uint i = 0; i < SampleCount; ++i)
    {
        vec2 Xi = Hammersley(i, SampleCount);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float nDotL = max(L.z, 0.0);
        float nDotH = max(H.z, 0.0);
        float vDotH = max(dot(V, H), 0.0);

        if(nDotL > 0.0)
        {
            float G = GeometrySmith(N, V, L, k);
            float GVis = (G * vDotH) / (nDotH * nDotV);
            float Fc = pow(1.0 - vDotH, 5);

            scale += (1.0 - Fc) * GVis;
            bias += Fc * GVis;
        }
    }
    scale /= SampleCount;
    bias /= SampleCount;

    outColor = vec4(scale, bias, 0.0, 1.0);
}