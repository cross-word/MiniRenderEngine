//***************************************************************************************
// LightingUtil.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Contains API for shader lighting.
//***************************************************************************************

#define MaxLights 25

struct Light
{
    float3 Color;   float Intensity;   // dir: lux, point/spot: cd
    float3 Direction; float Range;     // <=0: infinite
    float3 Position;  float InnerCos;  // spot only
    int    Type;     float OuterCos;   // spot only
    float2 _pad_;
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float  Shininess;
};

float3 SchlickFresnel(float3 R0, float3 N, float3 L)
{
    float c = saturate(dot(N, L));
    float f0 = 1.0 - c;
    return R0 + (1.0 - R0) * (f0 * f0 * f0 * f0 * f0);
}

float3 BlinnPhong(float3 Li, float3 L, float3 N, float3 V, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    float3 H = normalize(V + L);
    float  spec = (m + 8.0f) * pow(max(dot(H, N), 0.0f), m) / 8.0f;
    float3 F = SchlickFresnel(mat.FresnelR0, H, L);
    float3 specAlb = F * spec;
    specAlb = specAlb / (specAlb + 1.0f); // LDR 조정
    return (mat.DiffuseAlbedo.rgb + specAlb) * Li;
}

float DistanceAttenuation(float d, float range)
{
    float invSq = 1.0f / max(d * d, 1e-4f);
    if (range <= 0.0f) return invSq;
    float x = saturate(d / range);
    float smooth = (1.0f - x * x);
    return invSq * smooth * smooth;
}

float SpotAttenuation(float cosTheta, float innerCos, float outerCos)
{
    float t = saturate((cosTheta - outerCos) / max(1e-4f, (innerCos - outerCos)));
    return t * t;
}

// ------------------------------------------------------------
// Directional: E[lux] ≈ Intensity * N·L
float3 ComputeDirectionalLight(Light L, Material mat, float3 N, float3 V)
{
    float3 Ldir = -normalize(L.Direction);
    float  ndotl = max(dot(Ldir, N), 0.0f);
    float3 Li = L.Color * (L.Intensity * ndotl);
    return BlinnPhong(Li, Ldir, N, V, mat);
}

// Point: E ≈ I[cd] / d^2
float3 ComputePointLight(Light L, Material mat, float3 P, float3 N, float3 V)
{
    float3 Lvec = L.Position - P;
    float  d = length(Lvec);
    if (L.Range > 0.0f && d > L.Range) return 0.0f; //out of range
    float3 Ldir = Lvec / max(d, 1e-4f);
    float  ndotl = max(dot(Ldir, N), 0.0f);
    float  att = DistanceAttenuation(d, L.Range);
    float3 Li = L.Color * (L.Intensity * ndotl * att);
    return BlinnPhong(Li, Ldir, N, V, mat);
}

// Spot: Point * SpotShape
float3 ComputeSpotLight(Light L, Material mat, float3 P, float3 N, float3 V)
{
    float3 Lvec = L.Position - P;
    float  d = length(Lvec);
    if (L.Range > 0.0f && d > L.Range) return 0.0f;
    float3 Ldir = Lvec / max(d, 1e-4f);

    float  ndotl = max(dot(Ldir, N), 0.0f);
    float  attD = DistanceAttenuation(d, L.Range);

    float  cosTheta = dot(-Ldir, normalize(L.Direction));
    float  attS = SpotAttenuation(cosTheta, L.InnerCos, L.OuterCos);

    float3 Li = L.Color * (L.Intensity * ndotl * attD * attS);
    return BlinnPhong(Li, Ldir, N, V, mat);
}

// ------------------------------------------------------------
float4 ComputeLighting(Light gLights[MaxLights], Material mat,
    float3 pos, float3 normal, float3 toEye, float3 shadowFactor)
{
    float3 sumL = 0.0f;
    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    [unroll]
        for (i = 0; i < NUM_DIR_LIGHTS; ++i)
            sumL += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
#endif

#if (NUM_POINT_LIGHTS > 0)
    [unroll]
        for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
            sumL += ComputePointLight(gLights[i], mat, pos, normal, toEye);
#endif

#if (NUM_SPOT_LIGHTS > 0)
    [unroll]
        for (i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
            sumL += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
#endif

    return float4(sumL, 0.0f);
}
