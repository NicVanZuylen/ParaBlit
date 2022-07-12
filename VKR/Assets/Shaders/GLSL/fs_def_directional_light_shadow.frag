#version 450
#include "Common/pbr.h"
#include "Common/pb_common.h"
#include "Common/view_constants.h"
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
    uint viewConstantsIndex;
    uint lightingUBOIndex;
    uint colorRTVIdx;
    uint normalRTVIdx;
    uint specAndRoughRTVIdx;
    uint depthRTVIdx;
    uint shadowmaskIdx;
    uint aoIndex;
    uint irradianceIdx;
    uint prefilteredEnvMapIdx;
    uint specBDRFLutIdx;
    uint samplerIdx;
    uint iblSamplerIdx;
} PB_BINDINGS_NAME;

layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 0) uniform textureCube cubeTextures[];
layout(set = 0, binding = 1) uniform sampler samplers[];

DEFINE_VIEW_CONSTANTS(viewConstants);
#define VIEW_CONST PB_UBO(viewConstants, viewConstantsIndex)

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

//#define PI 3.14159265359f

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
    mat4 invView = VIEW_CONST.invView;
    mat4 invProj = VIEW_CONST.invProj;
    vec4 camPos  = VIEW_CONST.cameraPosition;

    vec3 gamma = vec3(1.0 / 2.2);

    vec4 colorTexel = texture
    (
        sampler2D(textures[nonuniformEXT(PB_BINDINGS_NAME.colorRTVIdx)], samplers[nonuniformEXT(PB_BINDINGS_NAME.samplerIdx)]), 
        fsInput.texCoord
    );
    vec3 color = colorTexel.rgb;
    float emissionMask = colorTexel.a;

    vec3 normal = texture
    (
        sampler2D(textures[nonuniformEXT(PB_BINDINGS_NAME.normalRTVIdx)], samplers[nonuniformEXT(PB_BINDINGS_NAME.samplerIdx)]), 
        fsInput.texCoord
    ).xyz;

    vec4 specAndRough = texture
    (
        sampler2D(textures[nonuniformEXT(PB_BINDINGS_NAME.specAndRoughRTVIdx)], samplers[nonuniformEXT(PB_BINDINGS_NAME.samplerIdx)]), 
        fsInput.texCoord
    );
    vec3 specular = specAndRough.rgb;
    float roughness = pow(specAndRough.a, gamma.r);

    float depth = texture
    (
        sampler2D(textures[nonuniformEXT(PB_BINDINGS_NAME.depthRTVIdx)], samplers[nonuniformEXT(PB_BINDINGS_NAME.samplerIdx)]), 
        fsInput.texCoord
    ).r;
    vec3 position = WorldPosFromDepth(depth, fsInput.texCoord, invView, invProj);

    vec3 dirToCam = -normalize(position - camPos.xyz);
    float normalDotCam = max(dot(normal, dirToCam), 0.0);

    float shadow = texture
    (
        sampler2D(textures[nonuniformEXT(PB_BINDINGS_NAME.shadowmaskIdx)], samplers[nonuniformEXT(PB_BINDINGS_NAME.samplerIdx)]), 
        fsInput.texCoord
    ).r;

    float ao = texture
    (
        sampler2D(textures[nonuniformEXT(PB_BINDINGS_NAME.aoIndex)], samplers[nonuniformEXT(PB_BINDINGS_NAME.samplerIdx)]), 
        fsInput.texCoord
    ).r;

    vec3 envIrradiance = texture
    (
        samplerCube(cubeTextures[nonuniformEXT(PB_BINDINGS_NAME.irradianceIdx)], samplers[nonuniformEXT(PB_BINDINGS_NAME.iblSamplerIdx)]), 
        normal
    ).rgb;
    envIrradiance = pow(envIrradiance, gamma);

    vec3 kS = FresnelShlickRoughness(normalDotCam, specular, roughness);
    vec3 kD = (1.0 - kS) * (1.0 - specular);
    vec3 ambientDiffuse = envIrradiance * color * kD;

    vec3 reflectionVec = reflect(-dirToCam, normal);
    float prefilterMipCount = float(textureQueryLevels(samplerCube(cubeTextures[nonuniformEXT(PB_BINDINGS_NAME.prefilteredEnvMapIdx)], samplers[nonuniformEXT(PB_BINDINGS_NAME.iblSamplerIdx)])));
    vec3 indirectSpecular = textureLod
    (
        samplerCube(cubeTextures[nonuniformEXT(PB_BINDINGS_NAME.prefilteredEnvMapIdx)], samplers[nonuniformEXT(PB_BINDINGS_NAME.iblSamplerIdx)]), 
        reflectionVec,
        roughness * prefilterMipCount
    ).rgb;
    vec2 specBDRF = texture
    (
        sampler2D(textures[nonuniformEXT(PB_BINDINGS_NAME.specBDRFLutIdx)], samplers[nonuniformEXT(PB_BINDINGS_NAME.samplerIdx)]), 
        vec2(normalDotCam, roughness)
    ).rg;
    indirectSpecular = pow(indirectSpecular, gamma);
    vec3 ambientSpecular = indirectSpecular * (kS * specBDRF.x + specBDRF.y);

    vec4 Lo = vec4((ambientDiffuse + ambientSpecular) * ao, 1.0);
    
    int count = lightingData[nonuniformEXT(PB_BINDINGS_NAME.lightingUBOIndex)].count;
    for(int i = 0; i < count; ++i)
    {
        DirectionalLight light = lightingData[nonuniformEXT(PB_BINDINGS_NAME.lightingUBOIndex)].lights[i];
        vec3 normLightDir = normalize(light.direction.xyz);

        float normalDotLight = max(dot(normal, normLightDir), 0.0);

        // float orenNayar = OrenNayarDiff(normal, normLightDir, dirToCam, roughness);
        // vec3 cookTorrence = CookTorrenceSpec(normal, normLightDir, dirToCam, normalDotLight, roughness, specular);

        // vec3 diffuse = orenNayar * light.color.rgb;
        // vec3 spec = cookTorrence * light.color.rgb;

        // Lo += vec4(((diffuse * color) + spec) * shadow, 0.0);

        vec3 radiance = light.color.xyz;
        vec3 kS;
        vec3 bdrfSpecular = CookTorranceDirect(normal, dirToCam, normLightDir, specular, kS, roughness, normalDotLight);

        vec3 kD = (1.0 - kS) * (1.0 - specular);
        Lo.rgb += Reflectance(color, bdrfSpecular, radiance, kD, normalDotLight) * shadow;
    }

    float emissionIntensityScale = lightingData[nonuniformEXT(PB_BINDINGS_NAME.lightingUBOIndex)].emissionIntensityScale;
    vec4 emissionOutput = vec4(color.rgb * emissionIntensityScale, 1.0);

    outColor = emissionMask == 0.0 ? Lo : emissionOutput;
}