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

#ifndef SHADOW_BIAS
#define SHADOW_BIAS 0.0015f
#endif
#define SHADOW_PCF_3x3


// Include structures and functions for lighting.
//#include "GltfLightingUtil.hlsl"
#include "Common.hlsl"
#include "LightingUtil.hlsl"

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

Texture2D    gDiffuseMap[NUM_TEXTURE] : register(t0, space0);
StructuredBuffer<MaterialParam> gMaterials : register(t0, space1);
StructuredBuffer<ObjectParam>   gObject    : register(t1, space1);
Texture2D gShadowMap : register(t2, space1);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

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

    float4x4 gLightViewProj;
    float2 gShadowTexelSize; float2 _padShadow;

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

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
    float4 TangentL : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    //float3 BitangentW : BINORMAL;
    float2 TexC : TEXCOORD;
};

float ComputeShadowFactorWS(float3 posWS)
{
    float4 posL = mul(float4(posWS, 1.0f), gLightViewProj);
    float  invW = rcp(max(abs(posL.w), 1e-6));
    float3 ndc = posL.xyz * invW;
    float2 uv = ndc.xy * 0.5f + 0.5f;
    float  zRef = ndc.z - SHADOW_BIAS;

    if (any(uv < 0.0) || any(uv > 1.0) || zRef <= 0.0 || zRef >= 1.0)
        return 1.0;

#ifdef SHADOW_PCF_3x3
    if (gShadowTexelSize.x > 0 && gShadowTexelSize.y > 0)
    {
        float sum = 0.0f;
        [unroll] for (int y = -1; y <= 1; ++y)
        {
            [unroll] for (int x = -1; x <= 1; ++x)
            {
                float2 off = float2(x, y) * gShadowTexelSize;
                sum += gShadowMap.SampleCmpLevelZero(gsamShadow, uv + off, zRef);
            }
        }
        return sum / 9.0f;
    }
#endif

    return gShadowMap.SampleCmpLevelZero(gsamShadow, uv, zRef);
}

VertexOut VSMain(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

    // Fetch the material data.
    MaterialParam matParam = gMaterials[gMaterialIndex];

    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gObject[gObjectId].gWorlds);
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3)gObject[gObjectId].gWorlds);

    vout.TangentW = mul(vin.TangentL.xyz, (float3x3)gObject[gObjectId].gWorlds);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

    // Output vertex attributes for interpolation across triangle.
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gObject[gObjectId].gTransform);
    vout.TexC = mul(texC, gMaterials[gMaterialIndex].gMatTransform).xy;

    return vout;
}

float4 PSMain(VertexOut pin) : SV_Target
{
    // Fetch the material data.
    MaterialParam matParam = gMaterials[gMaterialIndex];
    float4 diffuseAlbedo = matParam.gDiffuseAlbedo;
    float3 fresnelR0 = matParam.gFresnelR0;
    float  roughness = matParam.gMetallic;
    uint diffuseMapIndex = matParam.gBaseColorIdx;
    uint normalMapIndex = matParam.gNormalIdx;

    // Dynamically look up the texture in the array.
    diffuseAlbedo *= gDiffuseMap[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
    // Discard pixel if texture alpha < 0.1.  We do this test as soon 
    // as possible in the shader so that we can potentially exit the
    // shader early, thereby skipping the rest of the shader code.
    clip(diffuseAlbedo.a - 0.1f);
#endif

    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    float4 normalMapSample = gDiffuseMap[normalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
    float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);

    // Uncomment to turn off normal mapping.
    //bumpedNormalW = pin.NormalW;

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    // -------- 섀도우 계산 추가 --------
    float S = ComputeShadowFactorWS(pin.PosW); // 0~1

    // ComputeLighting은 directional에 한해 shadowFactor[i] 곱을 기대하므로,
    // NUM_DIR_LIGHTS 개수에 맞춰 채움(나머지는 1.0)
    float3 shadowFactor =
#if (NUM_DIR_LIGHTS >= 3)
        float3(S, S, S);
#elif (NUM_DIR_LIGHTS == 2)
        float3(S, S, 1.0);
#elif (NUM_DIR_LIGHTS == 1)
        float3(S, 1.0, 1.0);
#else
        float3(1.0, 1.0, 1.0);
#endif

    // ----------------------------------
    
    // Light terms.
    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = (1.0f - roughness) * normalMapSample.a;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        bumpedNormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // Add in specular reflections.
    float3 r = reflect(-toEyeW, bumpedNormalW);
    //float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);
    float3 fresnelFactor = SchlickFresnel(fresnelR0, bumpedNormalW, r);
    litColor.rgb += shininess * fresnelFactor;

    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    return litColor;
}