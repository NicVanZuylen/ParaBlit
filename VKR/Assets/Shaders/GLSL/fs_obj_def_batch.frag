#version 450
#extension GL_ARB_separate_shader_objects : enable

// Resources are accessed via index, and each input from the vertex shader may contain a unique set of textures and sampler.
#extension GL_EXT_nonuniform_qualifier : enable

layout(push_constant) uniform Bindings
{
    uint mvpIndex;
} bindings;

layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 1) uniform sampler samplers[];

layout (location = 0) in FS_IN
{
    vec4 position;
    vec2 texCoord;
    flat uint samplerIdx;
    flat uint vertexIdx;
    flat uint textureIndices[6];
    mat3 tbnMatrix;
} fsInput;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outSpecAndRough;
layout(location = 3) out vec4 outEmission;

void main() 
{
    uint samplerIdx = fsInput.samplerIdx;
    uint colorIdx = fsInput.textureIndices[0];
    uint normalIdx = fsInput.textureIndices[1];
    uint specIdx = fsInput.textureIndices[2];
    uint roughIdx = fsInput.textureIndices[3];
    uint emissionIdx = fsInput.textureIndices[4];

    vec4 color = texture
    (
        sampler2D(textures[nonuniformEXT(colorIdx)], samplers[nonuniformEXT(samplerIdx)]), 
        fsInput.texCoord
    );

    vec3 normal = texture
    (
        sampler2D(textures[nonuniformEXT(normalIdx)], samplers[nonuniformEXT(samplerIdx)]), 
        fsInput.texCoord
    ).xyz;

    vec3 spec = texture
    (
        sampler2D(textures[nonuniformEXT(specIdx)], samplers[nonuniformEXT(samplerIdx)]), 
        fsInput.texCoord
    ).rgb;

    float roughness = texture
    (
        sampler2D(textures[nonuniformEXT(roughIdx)], samplers[nonuniformEXT(samplerIdx)]), 
        fsInput.texCoord
    ).r;

    vec3 emission = texture
    (
        sampler2D(textures[nonuniformEXT(emissionIdx)], samplers[nonuniformEXT(samplerIdx)]), 
        fsInput.texCoord
    ).rgb;

    outColor = vec4(color.rgb, 1.0);
    outNormal = vec4(normalize(fsInput.tbnMatrix * (normal * 2.0 - 1.0)), 1.0);
    outSpecAndRough = vec4(spec, roughness);
    outEmission = vec4(emission, 1.0);
}