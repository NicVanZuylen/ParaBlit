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
    int mvpUBOIndex;
    int lightingUBOIndex;
    int colorRTVIdx;
    int normalRTVIdx;
    int specAndRoughRTVIdx;
    int depthRTVIdx;
    int samplerIdx;
} bindings;

layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 1) uniform sampler samplers[];

layout(set = 1, binding = 0) uniform MVP
{
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;
    vec4 cameraPosition;
} mvp[];

struct DirectionalLight
{
    vec4 direction;
    vec4 color;
};

layout(set = 1, binding = 0) uniform LightingData
{
    DirectionalLight lights[8];
    int count;
} lightingData[];

layout(location = 0) in FS_IN fsInput;

layout(location = 0) out vec4 outColor;

vec3 WorldPosFromDepth(float depth, vec2 texCoords, inout mat4 invView, inout mat4 invProj)
{
	// Convert x & y to clip space and include z.
	vec4 clipSpacePos = vec4(texCoords * 2.0f - 1.0f, depth, 1.0f);
	vec4 viewSpacepos = invProj * clipSpacePos; // Transform clip space position to view space.

	// Do perspective divide
	viewSpacepos /= viewSpacepos.w;

	// Transform to worldspace from viewspace.
	vec4 worldPos = invView * viewSpacepos;

	return worldPos.xyz;
}

#define SPECULAR_FOCUS 32
#define SPECULAR_POWER 3

void main() 
{
    mat4 invView = mvp[nonuniformEXT(bindings.mvpUBOIndex)].invView;
    mat4 invProj = mvp[nonuniformEXT(bindings.mvpUBOIndex)].invProj;
    vec4 camPos  = mvp[nonuniformEXT(bindings.mvpUBOIndex)].cameraPosition;

    vec3 color = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.colorRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    ).rgb;

    vec3 normal = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.normalRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    ).xyz;

    float depth = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.depthRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    ).r;
    vec3 position = WorldPosFromDepth(depth, fsInput.texCoord, invView, invProj);

    outColor = vec4(color * 0.1, 1.0);

    vec3 dirToCam = normalize(position - camPos.xyz);

    int count = lightingData[nonuniformEXT(bindings.lightingUBOIndex)].count;
    for(int i = 0; i < count; ++i)
    {
        DirectionalLight light = lightingData[nonuniformEXT(bindings.lightingUBOIndex)].lights[i];
        vec3 normLightDir = normalize(light.direction.xyz);

        float normalDotLight = max(dot(normal, normLightDir), 0.0);
        float normalDotCam = max(dot(normal, dirToCam), 0.0);

        vec3 lightReflected = reflect(normLightDir, normal);
        float specTerm = max(dot(lightReflected, dirToCam), 0.0);

        vec3 diffuse = normalDotLight * light.color.rgb;
        vec3 spec = max(normalDotLight * pow(specTerm, SPECULAR_FOCUS) * SPECULAR_POWER * light.color.rgb, 0.0);

        outColor += vec4((diffuse + spec) * color, 1.0);
    }
}