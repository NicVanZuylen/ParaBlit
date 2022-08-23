#version 450
#include "Common/pb_common.h"
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Bindings
{
    uint constantsIndex;
    uint srcCubeIndex;
    uint srcSamplerIndex;
} PB_BINDINGS_NAME;

PB_DEFINE_TEXTURE_CUBE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

layout(location = 0) in vec3 pos;

layout(location = 0) out vec4 outColor;

#define SRC_SAMPLER PB_BUILD_SAMPLER_CUBE(srcCubeIndex, srcSamplerIndex)

#define PI 3.141592

void main()
{
    vec3 irradiance = vec3(0.0);

    vec3 normal = normalize(pos) * vec3(1.0, -1.0, -1.0);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0;
    for(float phi = 0.0; phi < (2 * PI); phi += sampleDelta)
    {
        for(float theta = 0.0; theta < (0.5 * PI); theta += sampleDelta)
        {
            // Convert from spherical to tangent space.
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

            // Convert from tangent space to world space.
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += texture(SRC_SAMPLER, sampleVec).rgb * cos(theta) * sin(theta);
            ++nrSamples;
        }
    }

    irradiance = PI * irradiance * (1.0 / nrSamples);
    outColor = vec4(irradiance, 1.0);
}