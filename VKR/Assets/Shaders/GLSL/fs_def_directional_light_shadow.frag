#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_samplerless_texture_functions : require

struct FS_IN
{
    vec2 texCoord;
    vec2 pad0;
};

layout(push_constant) uniform Bindings
{
    uint mvpUBOIndex;
    uint lightingUBOIndex;
    uint colorRTVIdx;
    uint normalRTVIdx;
    uint specAndRoughRTVIdx;
    uint depthRTVIdx;
    uint shadowmaskIdx;
    uint aoIndex;
    uint skyboxIndex;
    uint samplerIdx;
    uint iblSamplerIdx;
} bindings;

layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 0) uniform textureCube cubeTextures[];
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
    float emissionIntensityScale;
} lightingData[];

layout(location = 0) in FS_IN fsInput;

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

//float CookTorrenceSpec(vec3 normal, vec3 lightDir, vec3 viewDir, float lambert, float roughness, float reflectionCoefficient) 
vec3 CookTorrenceSpec(vec3 normal, vec3 lightDir, vec3 viewDir, float lambert, float roughness, vec3 reflectivity) 
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
	//float F = reflectionCoefficient + (1.0f - reflectionCoefficient) * pow(1.0f - normalDotView, 5);
    vec3 F = reflectivity + (1.0f - reflectivity) * pow(1.0f - normalDotView, 5);

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

vec3 ScreenPosFromWorld(vec3 worldPosition, inout mat4 view, inout mat4 proj)
{
    vec4 screenPosRaw = proj * view * vec4(worldPosition, 1.0);
    return screenPosRaw.xyz / screenPosRaw.w;
}

vec3 FresnelShlick(float cosTheta, vec3 spec)
{
    return spec + (1.0 - spec) * pow(1.0 - cosTheta, 5.0);
}

vec3 FresnelShlickRoughness(float cosTheta, vec3 spec, float roughness)
{
    return spec + (max(vec3(1.0 - roughness), spec) - spec) * pow(1.0 - cosTheta, 5.0);
}

void main() 
{
    mat4 invView = mvp[nonuniformEXT(bindings.mvpUBOIndex)].invView;
    mat4 invProj = mvp[nonuniformEXT(bindings.mvpUBOIndex)].invProj;
    vec4 camPos  = mvp[nonuniformEXT(bindings.mvpUBOIndex)].cameraPosition;

    vec3 gamma = vec3(1.0 / 2.2);

    vec4 colorTexel = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.colorRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    );
    vec3 color = colorTexel.rgb;
    float emissionMask = colorTexel.a;

    vec3 normal = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.normalRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    ).xyz;

    vec4 specAndRough = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.specAndRoughRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    );
    vec3 specular = specAndRough.rgb;
    float roughness = pow(specAndRough.a, gamma.r);

    float depth = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.depthRTVIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    ).r;
    vec3 position = WorldPosFromDepth(depth, fsInput.texCoord, invView, invProj);

    vec3 dirToCam = -normalize(position - camPos.xyz);
    float normalDotCam = max(dot(normal, dirToCam), 0.0);

    float shadow = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.shadowmaskIdx)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    ).r;

    float ao = texture
    (
        sampler2D(textures[nonuniformEXT(bindings.aoIndex)], samplers[nonuniformEXT(bindings.samplerIdx)]), 
        fsInput.texCoord
    ).r;

    float mipCount = float(textureQueryLevels(samplerCube(cubeTextures[nonuniformEXT(bindings.skyboxIndex)], samplers[nonuniformEXT(bindings.iblSamplerIdx)])));
    vec3 indirectDiffuse = textureLod
    (
        samplerCube(cubeTextures[nonuniformEXT(bindings.skyboxIndex)], samplers[nonuniformEXT(bindings.iblSamplerIdx)]), 
        normal,
        mipCount - 1.0
    ).rgb;
    indirectDiffuse = pow(indirectDiffuse, gamma);

    vec3 ambientSpecPortion = FresnelShlickRoughness(normalDotCam, specular, roughness);
    vec3 ambientDiffusePortion = (1.0 - ambientSpecPortion) * (1.0 - specular);
    vec3 ambientDiffuse = indirectDiffuse * color * ambientDiffusePortion;

    vec3 reflectionVec = reflect(-dirToCam, normal);
    vec3 indirectSpecular = textureLod
    (
        samplerCube(cubeTextures[nonuniformEXT(bindings.skyboxIndex)], samplers[nonuniformEXT(bindings.iblSamplerIdx)]), 
        reflectionVec,
        roughness * mipCount
    ).rgb;
    indirectSpecular = pow(indirectSpecular, gamma);
    vec3 ambientSpecular = indirectSpecular * ambientSpecPortion;

    vec4 lightingColor = vec4((ambientDiffuse + ambientSpecular) * ao, 1.0);
    
    int count = lightingData[nonuniformEXT(bindings.lightingUBOIndex)].count;
    for(int i = 0; i < count; ++i)
    {
        DirectionalLight light = lightingData[nonuniformEXT(bindings.lightingUBOIndex)].lights[i];
        vec3 normLightDir = normalize(light.direction.xyz);

        float normalDotLight = max(dot(normal, normLightDir), 0.0);

        float orenNayar = OrenNayarDiff(normal, normLightDir, dirToCam, roughness);
        vec3 cookTorrence = CookTorrenceSpec(normal, normLightDir, dirToCam, normalDotLight, roughness, specular);

        vec3 diffuse = orenNayar * light.color.rgb;
        vec3 spec = cookTorrence * light.color.rgb;

        lightingColor += vec4(((diffuse * color) + spec) * shadow, 0.0);
    }

    float emissionIntensityScale = lightingData[nonuniformEXT(bindings.lightingUBOIndex)].emissionIntensityScale;
    vec4 emissionOutput = vec4(color.rgb * emissionIntensityScale, 1.0);

    outColor = emissionMask == 0.0 ? lightingColor : emissionOutput;
}