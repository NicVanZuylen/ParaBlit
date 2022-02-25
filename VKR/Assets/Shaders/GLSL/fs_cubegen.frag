#version 450
#include "Common/pb_common.h"
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Bindings
{
    uint constantsIndex;
    uint srcImageIndex;
    uint srcSamplerIndex;
} PB_BINDINGS_NAME;

PB_DEFINE_TEXTURE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

vec2 ProjSphereToCube(vec3 v)
{
    const vec2 invAtan = vec2(0.1591, 0.3183);
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

layout(location = 0) in vec3 pos;

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 texCoord = ProjSphereToCube(normalize(pos));
    vec3 color = texture(PB_BUILD_SAMPLER(srcImageIndex, srcSamplerIndex), texCoord).rgb;

    outColor = vec4(color, 1.0);
}