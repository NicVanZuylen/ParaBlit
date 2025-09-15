#version 450
#extension GL_ARB_separate_shader_objects : enable
#include "Common/pb_common.h"
#include "Common/view_constants.h"

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform Bindings
{
    uint viewConstantsIndex;
} PB_BINDINGS_NAME;

DEFINE_VIEW_CONSTANTS(viewConst);
#define VIEW_CONST PB_UBO(viewConst, viewConstantsIndex)

layout(location = 0) out vec4 outLineColor;

void main() 
{
    gl_Position = VIEW_CONST.viewProj * inPosition;
    outLineColor = inColor;
}