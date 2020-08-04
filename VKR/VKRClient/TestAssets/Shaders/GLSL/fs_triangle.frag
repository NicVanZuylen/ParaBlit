#version 450
#extension GL_ARB_separate_shader_objects : enable

struct FS_IN
{
    vec4 vtxColor;
};

layout(location = 0) in FS_IN fsInput;

layout(location = 0) out vec4 outColor;

void main() 
{
    outColor = fsInput.vtxColor;
}