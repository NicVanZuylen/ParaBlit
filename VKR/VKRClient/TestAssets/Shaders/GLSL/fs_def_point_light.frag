#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_samplerless_texture_functions : enable

struct FS_IN
{
    vec4 worldPos;
};

layout(push_constant) uniform Bindings
{
    int mvpUBOIndex;
    int lightingUBOIndex;
    int colorRTVIdx;
    int normalRTVIdx;
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

struct PointLight
{
    vec4 position;
    vec3 color;
    float radius;
};

layout(set = 1, binding = 0) uniform PointLightBuffer
{
    PointLight m_lights[512];
} lightingData[];

layout(location = 0) in FS_IN fsInput;
layout(location = 1) flat in int instanceIndex;

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

    ivec2 frameDimensions = textureSize(textures[nonuniformEXT(bindings.colorRTVIdx)], 0);
    vec2 texCoord = gl_FragCoord.xy;
    texCoord.x /= float(frameDimensions.x);
    texCoord.y /= float(frameDimensions.y);

    vec3 color = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.colorRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        texCoord
    ).rgb;

    vec3 normal = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.normalRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        texCoord
    ).xyz;

    float depth =
    (
        sampler2D(textures[nonuniformEXT(bindings.depthRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        texCoord
    ).r;
    vec3 position = WorldPosFromDepth(depth, texCoord, invView, invProj);

    outColor = vec4(color * 0.1, 1.0);

    PointLight light = lightingData[nonuniformEXT(bindings.lightingUBOIndex)].m_lights[instanceIndex];

    vec3 lightPosFinal = light.position.xyz;
    lightPosFinal *= -1;

    vec3 dirToCam = normalize(camPos.xyz - position);
    vec3 dirToLight = -normalize(lightPosFinal - position.xyz);

    float normalDotLight = max(dot(normal, dirToLight), 0.0);
    float normalDotCam = max(dot(normal, dirToCam), 0.0);

    // Attenuation function
    float dist = length(lightPosFinal - position);
    float attenuation = -pow((dist / light.radius) + 0.1f, 3) + 1;

    vec3 lightReflected = reflect(dirToLight, normal);
    float specTerm = max(dot(lightReflected, -dirToCam), 0.0);

    vec3 diffuse = normalDotLight * light.color;
    vec3 spec = normalDotLight * pow(specTerm, SPECULAR_FOCUS) * SPECULAR_POWER * light.color;

    //outColor += vec4((diffuse + spec) * attenuation * color, 1.0);
    outColor = vec4(color, 1.0);
}