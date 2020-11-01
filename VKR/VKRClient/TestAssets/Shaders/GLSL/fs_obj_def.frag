#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

struct FS_IN
{
    vec4 normal;
    vec4 position;
    vec2 texCoord;
    vec2 pad0;
    mat3 tbnMatrix;
};

layout(push_constant) uniform Bindings
{
    int mvpIndex;
    int colorIdx;
    int normalIdx;
    int specIdx;
    int roughIdx;
    int samplerIdx;
} bindings;

layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 1) uniform sampler samplers[];

layout(location = 0) in FS_IN fsInput;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outSpecAndRough;

void main() 
{
    vec4 color = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.colorIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    );

    vec3 normal = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.normalIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    ).xyz;

    vec3 spec = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.specIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    ).rgb;

    float roughness = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.roughIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    ).r;

    outColor = vec4(color.rgb, 1.0);
    //outNormal = fsInput.normal;
    outNormal = vec4(normalize(fsInput.tbnMatrix * (normal * 2.0 - 1.0)), 1.0);
    outSpecAndRough = vec4(spec, roughness);
}