//***************************************************************************************
// LightingUtil.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Contains API for shader lighting.
//***************************************************************************************

#define MaxLights 25

struct Light
{
    float3 Color;      float Intensity;
    float3 Direction;  float Range;
    float3 Position;   float InnerCos;
    int    Type;       float OuterCos;
    float2 _pad_;
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float Metallic;
};

float SpotAttenuation(float cosTheta, float innerCos, float outerCos)
{
    float denom = max(innerCos - outerCos, 1e-4);
    float t = saturate((cosTheta - outerCos) / denom);
    return t * t * (3.0 - 2.0 * t);
}

float RangeAttenuation(float d, float range)
{
    if (range <= 0.0f) return 1.0f;
    float x = saturate(d / range);
    return 1.0f - (x * x * (3.0 - 2.0 * x));
}

// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering 3rd Ed.").
// R0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0)*(f0*f0*f0*f0*f0);

    return reflectPercent;
}

static const float PI = 3.14159265f;

float D_GGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float NdotL, float NdotV, float roughness)
{
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

float3 MicrofacetBRDF(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    float3 halfVec = normalize(toEye + lightVec);
    float NdotL = saturate(dot(normal, lightVec));
    float NdotV = saturate(dot(normal, toEye));
    float NdotH = saturate(dot(normal, halfVec));
    float VdotH = saturate(dot(toEye, halfVec));
    float roughness = max(mat.Roughness, 0.001f);

    float3 F0 = lerp(mat.FresnelR0, mat.DiffuseAlbedo.rgb, mat.Metallic);
    float D = D_GGX(NdotH, roughness);
    float G = GeometrySmith(NdotL, NdotV, roughness);
    float3 F = SchlickFresnel(F0, halfVec, toEye);

    float3 numerator = D * G * F;
    float denom = 4.0f * NdotL * NdotV + 0.001f;
    float3 spec = numerator / denom;

    float3 kd = (1.0f - F) * (1.0f - mat.Metallic);
    float3 diffuse = kd * mat.DiffuseAlbedo.rgb / PI;

    return (diffuse + spec) * lightStrength;
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for directional lights.
//---------------------------------------------------------------------------------------
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction;

    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 irradiance = L.Color * L.Intensity;
    float3 lightStrength = irradiance * ndotl;

    return MicrofacetBRDF(lightStrength, lightVec, normal, toEye, mat);
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for point lights.
//---------------------------------------------------------------------------------------
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye, float shadowFactor)
{
    // The vector from the surface to the light.
    float3 lightVec = L.Position - pos;
    float  d = length(lightVec);
    if (L.Range > 0.0f && d > L.Range) return 0.0f;

    lightVec /= max(d, 1e-4);
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float invSq = 1.0f / max(d * d, 1e-6);
    float3 irradiance = L.Color * (L.Intensity * invSq);
    irradiance *= RangeAttenuation(d, L.Range);

    float3 lightStrength = irradiance * ndotl;

    return shadowFactor * MicrofacetBRDF(lightStrength, lightVec, normal, toEye, mat);
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for spot lights.
//---------------------------------------------------------------------------------------
float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye, float shadowFactor)
{
    float3 lightVec = L.Position - pos;
    float  d = length(lightVec);
    if (L.Range > 0.0f && d > L.Range) return 0.0f;

    lightVec /= max(d, 1e-4);
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float invSq = 1.0f / max(d * d, 1e-6);
    float3 irradiance = L.Color * (L.Intensity * invSq);
    irradiance *= RangeAttenuation(d, L.Range);

    float cosTheta = dot(-lightVec, L.Direction);
    float spot = SpotAttenuation(cosTheta, L.InnerCos, L.OuterCos);
    float3 lightStrength = irradiance * (ndotl * spot);

    return shadowFactor * MicrofacetBRDF(lightStrength, lightVec, normal, toEye, mat);
}

float4 ComputeLighting(Light gLights[MaxLights], Material mat,
                       float3 pos, float3 normal, float3 toEye,
                       float shadowFactor[MaxLights])
{
    float3 result = 0.0f;

    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    for(i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        result += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS+NUM_POINT_LIGHTS; ++i)
    {
        result += ComputePointLight(gLights[i], mat, pos, normal, toEye, shadowFactor[i]);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye, shadowFactor[i]);
    }
#endif 

    return float4(result, 0.0f);
}


