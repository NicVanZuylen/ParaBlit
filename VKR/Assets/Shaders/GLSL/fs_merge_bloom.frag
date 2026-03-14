#version 450
#include "Common/pb_common.h"
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_samplerless_texture_functions : require

layout(push_constant) uniform Bindings
{
    uint colorAIdx;
    uint colorBIdx;
    uint colorSamplerIdx;
} PB_BINDINGS_NAME;

PB_DEFINE_TEXTURE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

layout(location = 0) in VS_IN
{
    vec2 texCoord;
    vec2 pad0;
} vsInput;

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 screenTexCoord = vsInput.texCoord;
    vec4 colorA = texture(PB_BUILD_SAMPLER(colorAIdx, colorSamplerIdx), screenTexCoord);

    int mipCount = textureQueryLevels(PB_BUILD_SAMPLER(colorBIdx, colorSamplerIdx));
    vec4 colorB = vec4(0.0);
    for(int i = 0; i < mipCount; ++i)
    {
        colorB += textureLod(PB_BUILD_SAMPLER(colorBIdx, colorSamplerIdx), screenTexCoord, float(i));
    }

    colorA += colorB;

    const float exposure = 1.5;
    vec3 result = vec3(1.0) - exp(-colorA.rgb * exposure);

    outColor = vec4(result, 1.0);
}