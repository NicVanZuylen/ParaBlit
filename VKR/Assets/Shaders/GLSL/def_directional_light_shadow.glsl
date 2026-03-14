#version 450
#include "Common/pbr.h"
#include "Common/pb_common.h"
#include "Common/view_constants.h"
#include "Common/gbuffer_common.h"
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_samplerless_texture_functions : require

#define PERMUTATION_ShaderStage 2 // 0 == Vertex, 1 == Fragment
#define PERMUTATION_UseRTReflections 2 // 0 == FALSE, 1 == TRUE

// ********************************************************************************************************************************
#if PERMUTATION_ShaderStage == 0
// ********************************************************************************************************************************

vec2 positions[6] = vec2[]
(
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 1.0),
    vec2(-1.0, 1.0),
    vec2(-1.0, -1.0)
);

struct VS_OUT
{
    vec2 texCoord;
    vec2 pad0;
};

layout (location = 0) out VS_OUT vsOutput;

void main() 
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.01, 1.0);
    gl_Position = gl_Position.xyww;
    vsOutput.texCoord = (positions[gl_VertexIndex] + 1) * 0.5;
}

// ********************************************************************************************************************************
#endif
// ********************************************************************************************************************************

// ********************************************************************************************************************************
#if PERMUTATION_ShaderStage == 1
// ********************************************************************************************************************************

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
#if PERMUTATION_UseRTReflections == 0
    uint prefilteredEnvMapIdx;
#else
    uint reflectionTextureIndex;
#endif
    uint specBDRFLutIdx;
    uint samplerIdx;
    uint iblSamplerIdx;
} PB_BINDINGS_NAME;

PB_DEFINE_TEXTURE_BINDINGS;
PB_DEFINE_TEXTURE_CUBE_BINDINGS;
PB_DEFINE_SAMPLER_BINDINGS;

const vec3 gamma = vec3(1.0 / 2.2);

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
#define LIGHTING_DATA PB_UBO(lightingData, lightingUBOIndex)

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

vec3 CalcIndirectDiffuse(in vec3 kD, in vec3 surfaceColor, in vec3 surfaceNormal)
{
    vec3 envIrradiance = texture
    (
        samplerCube(PB_TEXTURE_CUBE(irradianceIdx), PB_SAMPLER(iblSamplerIdx)), 
        surfaceNormal
    ).rgb;
    return pow(envIrradiance, gamma) * surfaceColor * kD;
}

vec3 CalcIndirectSpecular(in vec3 kS, in vec3 dirToCam, in vec3 surfaceNormal, in float normalDotCam, in float surfaceRoughness)
{
    vec3 reflectionVec = reflect(-dirToCam, surfaceNormal);

#if PERMUTATION_UseRTReflections == 0
    float prefilterMipCount = float(textureQueryLevels(samplerCube(PB_TEXTURE_CUBE(prefilteredEnvMapIdx), PB_SAMPLER(iblSamplerIdx))));
    vec3 indirectSpecular = textureLod
    (
        samplerCube(PB_TEXTURE_CUBE(prefilteredEnvMapIdx), PB_SAMPLER(iblSamplerIdx)), 
        reflectionVec,
        surfaceRoughness * prefilterMipCount
    ).rgb;
#else
    vec3 indirectSpecular = texture
    (
        sampler2D(PB_TEXTURE(reflectionTextureIndex), PB_SAMPLER(samplerIdx)), 
        fsInput.texCoord
    ).rgb;
#endif

    vec2 specBDRF = texture
    (
        sampler2D(PB_TEXTURE(specBDRFLutIdx), PB_SAMPLER(samplerIdx)), 
        vec2(normalDotCam, surfaceRoughness)
    ).rg;
    indirectSpecular = pow(indirectSpecular, gamma);

    return indirectSpecular * (kS * specBDRF.x + specBDRF.y);
}

void main() 
{
    mat4 invView = VIEW_CONST.invView;
    mat4 invProj = VIEW_CONST.invProj;
    vec4 camPos  = VIEW_CONST.cameraPosition;

    vec4 colorTexel = texture
    (
        sampler2D(PB_TEXTURE(colorRTVIdx), PB_SAMPLER(samplerIdx)), 
        fsInput.texCoord
    );
    vec3 color = colorTexel.rgb;
    float emissionMask = colorTexel.a;

    vec3 normal = texture
    (
        sampler2D(PB_TEXTURE(normalRTVIdx), PB_SAMPLER(samplerIdx)), 
        fsInput.texCoord
    ).xyz;
    UnpackNormal(normal);

    vec4 specAndRough = texture
    (
        sampler2D(PB_TEXTURE(specAndRoughRTVIdx), PB_SAMPLER(samplerIdx)), 
        fsInput.texCoord
    );
    vec3 specular = specAndRough.rgb;
    float roughness = pow(specAndRough.a, gamma.r);

    float depth = texture
    (
        sampler2D(PB_TEXTURE(depthRTVIdx), PB_SAMPLER(samplerIdx)), 
        fsInput.texCoord
    ).r;
    vec3 position = WorldPosFromDepth(depth, fsInput.texCoord, invView, invProj);

    vec3 dirToCam = -normalize(position - camPos.xyz);
    float normalDotCam = max(dot(normal, dirToCam), 0.0);

    // TODO: Can shadow and AO exist in the same mask texture as different channels and be blurred together?
    // This could save us an extra render target, blur pass and subsequent sample in this pass.
    float shadow = texture
    (
        sampler2D(PB_TEXTURE(shadowmaskIdx), PB_SAMPLER(samplerIdx)), 
        fsInput.texCoord
    ).r;

    float ao = texture
    (
        sampler2D(PB_TEXTURE(aoIndex), PB_SAMPLER(samplerIdx)), 
        fsInput.texCoord
    ).r;

    vec3 kS = FresnelShlickRoughness(normalDotCam, specular, roughness);
    vec3 kD = (1.0 - kS) * (1.0 - specular);
    vec3 ambientDiffuse = CalcIndirectDiffuse(kD, color, normal);
    vec3 ambientSpecular = CalcIndirectSpecular(kS, dirToCam, normal, normalDotCam, roughness);

    vec4 lightingOutput = vec4((ambientDiffuse + ambientSpecular) * ao, 1.0);

    int count = LIGHTING_DATA.count;
    for(int i = 0; i < count; ++i)
    {
        DirectionalLight light = LIGHTING_DATA.lights[i];
        vec3 normLightDir = normalize(light.direction.xyz);

        float normalDotLight = max(dot(normal, normLightDir), 0.0);

        vec3 radiance = light.color.xyz;
        //vec3 kS;
        vec3 bdrfSpecular = CookTorranceDirect(normal, dirToCam, normLightDir, specular, kS, roughness, normalDotLight);

        vec3 kD = (1.0 - kS) * (1.0 - specular);
        lightingOutput.rgb += Reflectance(color, bdrfSpecular, radiance, kD, normalDotLight) * shadow;
    }

    float emissionIntensityScale = LIGHTING_DATA.emissionIntensityScale;

    vec3 emissionDecoded = DecodeColorF(color.rgb, 10);
    vec4 emissionOutput = vec4(emissionDecoded * emissionIntensityScale, 1.0);

    outColor = emissionMask == 0.0 ? lightingOutput : emissionOutput;
}

// ********************************************************************************************************************************
#endif
// ********************************************************************************************************************************