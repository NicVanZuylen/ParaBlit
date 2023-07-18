#version 450
#include "Common/pb_common.h"
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Bindings
{
    uint constantsIndex;
} PB_BINDINGS_NAME;

layout(set = 1, binding = 0) uniform ConstantsLayout
{
    mat4 proj;
    mat4 view;
} constants[];

#define CONSTANTS PB_UBO(constants, constantsIndex)

struct VS_IN
{
    vec4 position;
    vec4 color;
};

layout (location = 0) in VS_IN vsInput;

layout (location = 0) out vec3 outColor;

void main() 
{
    gl_Position = CONSTANTS.proj * CONSTANTS.view * vec4(vsInput.position.xyz, 1.0);

    // Random 
    outColor = vsInput.color.rgb;
}