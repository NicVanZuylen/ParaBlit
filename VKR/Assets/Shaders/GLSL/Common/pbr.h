
#ifndef PI
#define PI 3.141592
#endif

// ----------------------------------------------------------------------------
// BDRF
// ----------------------------------------------------------------------------

// Trowbridge-Reitz GGX Normal Distribution Function
// Where:
// N = surface normal
// H = halfway vector
// a = surface roughness
float DistributionGGX(in vec3 N, in vec3 H, float a)
{
    a *= a;
    float a2 = a * a;
    float nDH = max(dot(N, H), 0.0);
    float nDH2 = nDH * nDH;

    float nom = a2;
    float denom = (nDH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// Fresnel-Schlick Fresnel Function
vec3 FresnelSchlick(float cosTheta, in vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick for surface roughness
vec3 FresnelShlickRoughness(float cosTheta, vec3 spec, float roughness)
{
    return spec + (max(vec3(1.0 - roughness), spec) - spec) * pow(1.0 - cosTheta, 5.0);
}

// Smith's Schlick GGX Geometry Function
// Where:
// NDotV = dot product of surface normal and view vector
// k = approximation of roughness, which varies between direct lighting and IBL.
float GeometrySchlickGGX(float nDotV, float k)
{
    float num = nDotV;
    float denom = nDotV * (1.0 - k) + k;

    return num / denom;
}

// Where:
// N = surface normal
// V = view vector
// L = light vector
// k = approximation of roughness, which varies between direct lighting and IBL.
float GeometrySmith(in vec3 N, in vec3 V, in vec3 L, float k)
{
    float nDotV = max(dot(N, V), 0.0);
    float nDotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(nDotV, k);
    float ggx2 = GeometrySchlickGGX(nDotL, k);

    return ggx1 * ggx2;
}

vec3 CookTorranceDirect(in vec3 N, in vec3 V, in vec3 L, in vec3 F0, out vec3 kS, float a, float nDotL)
{
	vec3 H = normalize(L + V);
    float cosTheta = max(dot(H, V), 0.0);
    float a2 = a * a;
    float k = a2 + 1.0;
    k = (k * k) / 8.0;

    float D = DistributionGGX(N, H, a);
    vec3  F = FresnelSchlick(cosTheta, F0);
    float G = GeometrySmith(N, V, L, k);

    kS = F;

    float denom = 4.0 * max(dot(N, V), 0.0) * nDotL + 0.0001;
    return (D * F * G) / denom;
}

vec3 Reflectance(in vec3 albedo, in vec3 specular, in vec3 radiance, in vec3 kD, float nDotL)
{
    return ((kD * albedo / PI) + specular) * radiance * nDotL;
}
