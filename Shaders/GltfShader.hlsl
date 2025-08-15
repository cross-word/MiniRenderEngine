//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_LIGHTS
    #define NUM_LIGHTS 32
#endif

#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 24
#endif


#ifndef LIGHT_TYPE_DIRECTIONAL
    #define LIGHT_TYPE_DIRECTIONAL 0
#endif

#ifndef LIGHT_TYPE_POINT
    #define LIGHT_TYPE_POINT 1
#endif

#ifndef LIGHT_TYPE_SPOT
    #define LIGHT_TYPE_SPOT 2
#endif

// Include structures and functions for lighting.
#include "GltfLightingUtil.hlsl"

Texture2D    gDiffuseMap[NUM_TEXTURE] : register(t0, space0);

struct MaterialParam
{
    float4   gDiffuseAlbedo;       // baseColorFactor
    float3   gFresnelR0;  float gRoughness;
    float4x4 gMatTransform;

    float    gMetallic;
    float    gNormalScale;
    float    gOcclusionStrength;
    float    gEmissiveStrength;

    float3   gEmissiveFactor;

    uint     gBaseColorIdx;
    uint     gNormalIdx;
    uint     gORMIdx;          // G=Roughness, B=Metallic
    uint     gOcclusionIndex;  // R=AO
    uint     gEmissiveIdx;
};

struct ObjectParam
{
    float4x4 gWorlds;
    float4x4 gTransform;
};

StructuredBuffer<MaterialParam> gMaterials : register(t0, space1);
StructuredBuffer<ObjectParam>   gObject    : register(t1, space1);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per frame.
cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
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

cbuffer cbMaterialIndex : register(b2) {
    uint gMaterialIndex;
    uint gTexIndex;
    uint gObjectId;
};

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
    float4 TangentL : TANGENT;
};

struct VertexOut {
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float3 BitangentW : BINORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VSMain(VertexIn vin)
{
    VertexOut v = (VertexOut)0;

    float4 posW = mul(float4(vin.PosL, 1), gObject[gObjectId].gWorlds);
    v.PosW = posW.xyz;

    float3x3 world3 = transpose((float3x3)gObject[gObjectId].gWorlds);   // = W
    float3x3 normalMat = transpose(inverse3x3(world3));                   // = (W^-1)^T

    v.NormalW = normalize(mul(vin.NormalL, normalMat));
    v.TangentW = normalize(mul(vin.TangentL.xyz, world3));
    v.BitangentW = normalize(cross(v.NormalW, v.TangentW) * vin.TangentL.w);

    v.PosH = mul(posW, gViewProj);
    v.TexC = mul(float4(vin.TexC, 0, 1), gObject[gObjectId].gTransform).xy;
    v.TexC = mul(float4(v.TexC, 0, 1), gMaterials[gMaterialIndex].gMatTransform).xy;
    return v;
}

float4 PSMain(VertexOut pin) : SV_Target
{
    MaterialParam M = gMaterials[gMaterialIndex];

    // BaseColor
    if (M.gBaseColorIdx >= NUM_TEXTURE) return float4(1,1,1,1);
    float2 uv = pin.TexC;
    float4 baseSRGB = gDiffuseMap[NonUniformResourceIndex(M.gBaseColorIdx)]
                        .Sample(gsamAnisotropicWrap, uv);
    float3 baseColor = (baseSRGB * M.gDiffuseAlbedo).rgb;

    // MR Linear
    float rough = saturate(M.gRoughness);
    float metal = saturate(M.gMetallic);
    if (M.gORMIdx < NUM_TEXTURE) {
        float2 gb = gDiffuseMap[NonUniformResourceIndex(M.gORMIdx)]
            .Sample(gsamLinearWrap, uv).gb;
        rough = saturate(rough * gb.x);
        metal = saturate(metal * gb.y);
    }

    // AO Linear
    float ao = 1.0;
    if (M.gOcclusionIndex < NUM_TEXTURE) {
        float occ = gDiffuseMap[NonUniformResourceIndex(M.gOcclusionIndex)]
            .Sample(gsamLinearWrap, uv).r;
        ao = lerp(1.0, occ, M.gOcclusionStrength);
    }

    // Normal Linear
    float3 N = normalize(pin.NormalW);
    if (M.gNormalIdx < NUM_TEXTURE) {
        float3 nTS = gDiffuseMap[NonUniformResourceIndex(M.gNormalIdx)]
            .Sample(gsamAnisotropicWrap, pin.TexC).xyz * 2.0 - 1.0;
        nTS.xy *= M.gNormalScale;

        float3 N = normalize(nTS.x * pin.TangentW + nTS.y * pin.BitangentW + nTS.z * pin.NormalW);
    }

    // lights
    float3 V = normalize(gEyePosW - pin.PosW);
    float3 Lo = 0;
    #if (NUM_DIR_LIGHTS > 0)
        [unroll] for (int i = 0; i < NUM_DIR_LIGHTS; ++i) {
            float3 L = normalize(-gLights[i].Direction);
            float3 E = gLights[i].Color * gLights[i].Intensity; // lux
            Lo += DirectPBR(L, E, N, V, baseColor, metal, rough);
        }
    #endif
    #if (NUM_POINT_LIGHTS > 0)
        [unroll] for (int i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i) {
            float3 Lvec = gLights[i].Position - pin.PosW;
            float  d = length(Lvec); float3 L = Lvec / max(d,1e-4);
            float3 E = gLights[i].Color * (gLights[i].Intensity / max(d * d,1e-6));
            E *= RangeAttenuation(d, gLights[i].Range);
            Lo += DirectPBR(L, E, N, V, baseColor, metal, rough);
        }
    #endif
    #if (NUM_SPOT_LIGHTS > 0)
        [unroll] for (int i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i) {
            float3 Lvec = gLights[i].Position - pin.PosW;
            float  d = length(Lvec); float3 L = Lvec / max(d,1e-4);
            float3 E = gLights[i].Color * (gLights[i].Intensity / max(d * d,1e-6));
            E *= RangeAttenuation(d, gLights[i].Range);
            float ct = dot(-L, gLights[i].Direction);
            float spot = SpotAttenuation(ct, gLights[i].InnerCos, gLights[i].OuterCos);
            Lo += DirectPBR(L, E * spot, N, V, baseColor, metal, rough);
        }
    #endif

    float3 ambient = gAmbientLight.rgb * baseColor * ao;
    float3 emissive = 0;
    if (M.gEmissiveIdx < NUM_TEXTURE) {
        float3 e = gDiffuseMap[NonUniformResourceIndex(M.gEmissiveIdx)]
                        .Sample(gsamLinearWrap, uv).rgb;
        emissive = e * M.gEmissiveFactor * M.gEmissiveStrength;
    }

    float3 colorLin = ambient + Lo + emissive;
    colorLin = colorLin / (1.0 + colorLin);
    float3 outSRGB = pow(saturate(colorLin), 1.0 / 2.2);
    return float4(outSRGB, baseSRGB.a);
}