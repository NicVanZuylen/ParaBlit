#version 450
#include "Common/pb_common.h"

#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Bindings
{
    uint mvpUBOIndex;
    uint skyboxTexIndex;
    uint skySamplerIndex;
} PB_BINDINGS_NAME;

PB_DEFINE_TEXTURE_CUBE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

void main() 
{
    vec3 skyColor = texture(PB_BUILD_SAMPLER_CUBE(skyboxTexIndex, skySamplerIndex), inPosition).rgb;
    outColor = vec4(skyColor.rgb, 1.0);
    outColor.rgb = pow(outColor.rgb, vec3(1.0 / 2.2)); // Gamma correction.
}