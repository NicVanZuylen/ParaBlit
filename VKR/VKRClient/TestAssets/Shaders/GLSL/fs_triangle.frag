#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

struct FS_IN
{
    vec4 vtxColor;
};

layout(location = 0) in FS_IN fsInput;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform BindingIndices 
{
    int buf1Idx;
    int buf2Idx;
} bindingIndices;

layout(set = 1, binding = 0) uniform texture2D textures[];

//layout(set = 2, binding = 0) uniform UBOColor
//{
//    vec4 color1;
//    vec4 color2;
//} uboColor[];

//layout(set = 2, binding = 0) uniform UBOColor2
//{
//    vec4 color;
//} uboColor2[];

void main() 
{
    outColor = fsInput.vtxColor;
}