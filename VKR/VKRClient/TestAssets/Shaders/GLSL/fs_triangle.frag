#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

struct FS_IN
{
    vec2 texCoord;
    vec2 pad0;
};

layout(push_constant) uniform Bindings
{
    int mvpIndex;
    int colorIdx;
    int samplerIdx;
} bindings;

layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 1) uniform sampler samplers[];

layout(location = 0) in FS_IN fsInput;

layout(location = 0) out vec4 outColor;

void main() 
{
    vec4 color = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.colorIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    );
    outColor = vec4(color.rgb, 1.0f);
}