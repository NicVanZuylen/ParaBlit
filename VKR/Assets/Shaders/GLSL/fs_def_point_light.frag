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

float OrenNayarDiff(vec3 normal, vec3 lightDir, vec3 surfToCam, float roughness) 
{
    float roughSqr = roughness * roughness;

    float A = 1.0f - (0.5f * (roughSqr / (roughSqr + 0.33f)));
	float B = 0.45f * (roughSqr / (roughSqr + 0.09f));

	float normalDotLight = dot(normal, lightDir);
	float normalDotSurfToCam = dot(normal, surfToCam);

	vec3 lightProj = normalize(lightDir - (normal * normalDotLight));
	vec3 viewProj = normalize(surfToCam - (normal * normalDotSurfToCam));

	float cx = max(dot(lightProj, viewProj), 0.0f);

	float alpha =  sin(max(acos(normalDotSurfToCam), acos(normalDotLight)));
	float beta = tan(min(acos(normalDotSurfToCam), acos(normalDotLight)));

	float dx = alpha * beta;

    normalDotLight = max(normalDotLight, 0.0f);
	return clamp(normalDotLight * (A + B * cx * dx), 0.0f, 1.0f);
}

#define PI 3.14159265359f

float CookTorrenceSpec(vec3 normal, vec3 lightDir, vec3 viewDir, float lambert, float roughness, float reflectionCoefficient) 
{
    float roughSqr = roughness * roughness;

	float normalDotView = max(dot(normal, viewDir), 0.0f);

	vec3 halfVec = normalize(lightDir + viewDir);

	float normalDotHalf = max(dot(normal, halfVec), 0.0f);
	float normalDotHalfSqr = normalDotHalf * normalDotHalf;

	// Beckmann Distribution
	float exponent = -(1.0f - normalDotHalfSqr) / (normalDotHalfSqr * roughSqr);
	float D = exp(exponent) / (roughSqr * normalDotHalfSqr * normalDotHalfSqr);

	// Fresnel Term using Sclick's approximation.
	float F = reflectionCoefficient + (1.0f - reflectionCoefficient) * pow(1.0f - normalDotView, 5);

	// Geometric Attenuation Factor
	float halfFrac = 2.0f * normalDotHalf / dot(viewDir, halfVec);
	float G = min(1.0f, min(halfFrac * normalDotView, halfFrac * lambert));

	float bottomHalf = PI * normalDotView;

	return max((D * F * G) / bottomHalf, 0.0f);
}

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

// Similar to common attenuation (1/d) but with 0.1 added to dist to avoid the sudden drop in attenuation at one tenth of the light multiplier (or radius) in distance.
// Uses a natural logarithmic approach to help determine attenuation using a single multipler/radius.
// This function uses 1/d instead of 1/d^2 so attenuation isn't quite as harsh over distance.
float AttenuateLight(float dist, float lightMultiplier)
{
    float attn = -log((dist + 0.1) / lightMultiplier);
    return clamp(attn, 0.0, 1.0);
}

// Similar to real-world attenuation (1/d^2) but with 0.1 added to dist to avoid the sudden drop in attenuation at one tenth of the light multiplier (or radius) in distance.
// Uses a natural logarithmic approach to help determine attenuation using a single multipler/radius.
float AttenuateLightRealistic(float dist, float lightMultiplier)
{
    float attn = pow(log(min((dist + 0.1), lightMultiplier) / lightMultiplier), 2);
    return clamp(attn, 0.0, 1.0);
}

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

    vec4 specAndRough = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.specAndRoughRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        texCoord
    );
    vec3 specular = specAndRough.rgb;
    float roughness = specAndRough.a;

    float depth = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.depthRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        texCoord
    ).r;
    vec3 position = WorldPosFromDepth(depth, texCoord, invView, invProj);

    PointLight light = lightingData[nonuniformEXT(bindings.lightingUBOIndex)].m_lights[instanceIndex];

    vec3 dirToCam = normalize(camPos.xyz - position);
    vec3 dirToLight = normalize(light.position.xyz - position.xyz);

    float normalDotLight = max(dot(normal, dirToLight), 0.0);
    float normalDotCam = max(dot(normal, dirToCam), 0.0);

    float orenNayar = OrenNayarDiff(normal, dirToLight, dirToCam, roughness);
    float cookTorrence = CookTorrenceSpec(normal, dirToLight, dirToCam, normalDotLight, roughness, 1.0);

    // Attenuation function
    float dist = length(light.position.xyz - position);
    float attenuation = AttenuateLight(dist, light.radius);

    vec3 diffuse = orenNayar * light.color;
    vec3 spec = cookTorrence * specular * light.color;

    outColor = vec4(((diffuse * color) + spec) * attenuation, 1.0);
}