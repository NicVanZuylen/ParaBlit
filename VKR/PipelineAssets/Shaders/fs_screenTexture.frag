#version 450
#include "Common/pb_common.h"

struct FS_IN
{
    vec2 texCoord;
    vec2 pad0;
};

layout(push_constant) uniform Bindings
{
    uint srcTextureIndex;
    uint srcSamplerIndex;
} PB_BINDINGS_NAME;

PB_DEFINE_TEXTURE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

layout(location = 0) in FS_IN fsInput;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 colorSample = texture(PB_BUILD_SAMPLER(srcTextureIndex, srcSamplerIndex), fsInput.texCoord).xyz;
    outColor = vec4(colorSample, 1.0);
}