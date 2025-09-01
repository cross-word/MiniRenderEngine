//***************************************************************************************
// Common.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 24
#endif

#ifndef NUM_TEXTURE
#define NUM_TEXTURE 72
#endif

#ifndef NUM_LIGHTS
#define NUM_LIGHTS 25
#endif
// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

struct MaterialParam
{
    float4   DiffuseAlbedo;       // baseColorFactor
    float3   FresnelR0;  float Roughness;
    float4x4 MatTransform;

    float    gMetallic;
    float    gNormalScale;
    float    gOcclusionStrength;
    float    gEmissiveStrength;

    float3   gEmissiveFactor;

    uint     DiffuseMapIndex;
    uint     NormalMapIndex;
    uint     gORMIdx;          // G=Roughness, B=Metallic
    uint     gOcclusionIndex;  // R=AO
    uint     gEmissiveIdx;
};

struct ObjectParam
{
    float4x4 gWorlds;
    float4x4 gWorldInverseTranspose;
    float4x4 gTransform;
};

Texture2D    gTextureMapsSRGB[NUM_TEXTURE] : register(t0, space0);
Texture2D    gTextureMapsLinear[NUM_TEXTURE] : register(t0, space2);
StructuredBuffer<MaterialParam> gMaterialData : register(t0, space1);
StructuredBuffer<ObjectParam>   gObject    : register(t1, space1);
Texture2D gShadowMap : register(t2, space1);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gShadowTransform;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[NUM_LIGHTS];
};

cbuffer cbMaterialIndex : register(b2)
{
    uint gMaterialIndex;
    uint gTexIndex;
    uint gObjectId;
};

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float4 tangent, float normalScale)
{
    float2 rg = normalMapSample.xy * 2.0f - 1.0f;
    rg.y = -rg.y;
    rg *= normalScale;
    float  z = sqrt(saturate(1.0f - dot(rg, rg)));
    float3 normalT = normalize(float3(rg, z));

    // Uncompress each component from [0,1] to [-1,1].
    //float3 normalT = 2.0f*normalMapSample - 1.0f;

    // Build orthonormal basis.

    float3 N = normalize(unitNormalW);
    float3 T = tangent.xyz;
    float tLen2 = dot(T, T);
    if (tLen2 < 1e-6 || dot(N, N) < 1e-6)
    {
        return N;
    }
    T = normalize(T);
    T = normalize(T - N * dot(T, N));       // ±×·¥-½´¹ÌÆ®
    float3 B = tangent.w * normalize(cross(N, T));

    float3x3 TBN = float3x3(T, B, N);

    // Transform from tangent space to world space.
    float3 bumpedNormalW = normalize(mul(normalT, TBN));

    return bumpedNormalW;
}

//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float)width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
    };

    [unroll]
        for (int i = 0; i < 9; ++i)
        {
            percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
                shadowPosH.xy + offsets[i], depth).r;
        }

    return percentLit / 9.0f;
}

