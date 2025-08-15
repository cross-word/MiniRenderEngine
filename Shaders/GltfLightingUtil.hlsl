//***************************************************************************************
// LightingUtil.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Contains API for shader lighting.
//***************************************************************************************

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
    float Shininess;
};

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    // Linear falloff.
    return saturate((falloffEnd-d) / (falloffEnd - falloffStart));
}

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
    return 1.0f - (x * x * (3.0 - 2.0 * x)); // smoothstep(1-x)
}

float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0)*(f0*f0*f0*f0*f0);

    return reflectPercent;
}

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0f)*pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor*roughnessFactor;

    // Our spec formula goes outside [0,1] range, but we are 
    // doing LDR rendering.  So scale it down a bit.
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for directional lights.
//---------------------------------------------------------------------------------------
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction;
    float ndotl = max(dot(lightVec, normal), 0.0f);

    // irradiance (lux) = Color * Intensity
    float3 irradiance = L.Color * L.Intensity;
    float3 lightStrength = irradiance * ndotl;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for point lights.
//---------------------------------------------------------------------------------------
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;
    float  d = length(lightVec);
    if (L.Range > 0.0f && d > L.Range) return 0.0f;

    lightVec /= max(d, 1e-4);
    float ndotl = max(dot(lightVec, normal), 0.0f);

    // illuminance ≈ (cd / r^2)
    float invSq = 1.0f / max(d * d, 1e-6);
    float3 irradiance = L.Color * (L.Intensity * invSq) /* * gPtSpScale */;
    irradiance *= RangeAttenuation(d, L.Range);

    float3 lightStrength = irradiance * ndotl;
    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for spot lights.
//---------------------------------------------------------------------------------------
float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;
    float  d = length(lightVec);
    if (L.Range > 0.0f && d > L.Range) return 0.0f;

    lightVec /= max(d, 1e-4);
    float ndotl = max(dot(lightVec, normal), 0.0f);

    float invSq = 1.0f / max(d * d, 1e-6);
    float3 irradiance = L.Color * (L.Intensity * invSq) /* * gPtSpScale */;
    irradiance *= RangeAttenuation(d, L.Range);

    // cone
    float cosTheta = dot(-lightVec, L.Direction);
    float spot = SpotAttenuation(cosTheta, L.InnerCos, L.OuterCos);
    float3 lightStrength = irradiance * (ndotl * spot);

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float4 ComputeLighting(Light gLights[NUM_LIGHTS], Material mat,
                       float3 pos, float3 normal, float3 toEye,
                       float3 shadowFactor)
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
        result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
    }
#endif 

    return float4(result, 0.0f);
}


static const float PI = 3.14159265359;

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    // Schlick approx
    return F0 + (1.0 - F0) * pow(1.0 - saturate(cosTheta), 5.0);
}

float D_GGX(float NdotH, float a) // a = alpha = roughness^2
{
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-7);
}

float G_SchlickSmith(float NdotV, float NdotL, float a)
{
    // UE4 style k = (a+1)^2 / 8
    float k = (a + 1.0); k = (k * k) * 0.125;
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

float3 DirectPBR(float3 Ldir, float3 Lradiance,
    float3 N, float3 V,
    float3 baseColor, float metallic, float roughness)
{
    float3 L = normalize(Ldir);
    float3 H = normalize(V + L);
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));
    if (NdotL <= 0.0 || NdotV <= 0.0) return 0.0;

    float a = max(0.001, roughness * roughness);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);

    float3 F = FresnelSchlick(VdotH, F0);
    float  D = D_GGX(NdotH, a);
    float  G = G_SchlickSmith(NdotV, NdotL, a);

    float3 kd = (1.0 - F) * (1.0 - metallic);

    float3 diffuse = kd * baseColor / PI;
    float3 specular = (D * G) * F / max(4.0 * NdotL * NdotV, 1e-6);

    return (diffuse + specular) * (Lradiance * NdotL);
}

float det2(float a, float b, float c, float d)
{
    return a * d - b * c;
}

float det3(float a1, float a2, float a3,
    float b1, float b2, float b3,
    float c1, float c2, float c3)
{
    return a1 * (b2 * c3 - b3 * c2) - a2 * (b1 * c3 - b3 * c1) + a3 * (b1 * c2 - b2 * c1);
}

static const float kEps = 1e-8;
float3x3 inverse3x3(float3x3 m)
{
    float3 a = float3(m[0][0], m[1][0], m[2][0]);
    float3 b = float3(m[0][1], m[1][1], m[2][1]);
    float3 c = float3(m[0][2], m[1][2], m[2][2]);

    float3 r0 = cross(b, c);
    float3 r1 = cross(c, a);
    float3 r2 = cross(a, b);

    float det = dot(a, r0);
    float invDet = 1.0 / max(abs(det), kEps) * sign(det);

    // adj(m) = transpose([r0 r1 r2])
    return transpose(float3x3(r0, r1, r2)) * invDet;
}

float4x4 inverse4x4(float4x4 m)
{
    float c00 = det3(m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], m[3][3]);
    float c01 = -det3(m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], m[3][3]);
    float c02 = det3(m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], m[3][3]);
    float c03 = -det3(m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]);

    float c10 = -det3(m[0][1], m[0][2], m[0][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], m[3][3]);
    float c11 = det3(m[0][0], m[0][2], m[0][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], m[3][3]);
    float c12 = -det3(m[0][0], m[0][1], m[0][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], m[3][3]);
    float c13 = det3(m[0][0], m[0][1], m[0][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]);

    float c20 = det3(m[0][1], m[0][2], m[0][3], m[1][1], m[1][2], m[1][3], m[3][1], m[3][2], m[3][3]);
    float c21 = -det3(m[0][0], m[0][2], m[0][3], m[1][0], m[1][2], m[1][3], m[3][0], m[3][2], m[3][3]);
    float c22 = det3(m[0][0], m[0][1], m[0][3], m[1][0], m[1][1], m[1][3], m[3][0], m[3][1], m[3][3]);
    float c23 = -det3(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[3][0], m[3][1], m[3][2]);

    float c30 = -det3(m[0][1], m[0][2], m[0][3], m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], m[2][3]);
    float c31 = det3(m[0][0], m[0][2], m[0][3], m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], m[2][3]);
    float c32 = -det3(m[0][0], m[0][1], m[0][3], m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], m[2][3]);
    float c33 = det3(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]);

    float det = m[0][0] * c00 + m[0][1] * c01 + m[0][2] * c02 + m[0][3] * c03;
    float invDet = 1.0 / max(abs(det), kEps) * sign(det);

    // adj(m) = transpose(cofactor(m))
    float4x4 adjT = float4x4(
        c00, c10, c20, c30,
        c01, c11, c21, c31,
        c02, c12, c22, c32,
        c03, c13, c23, c33
    );

    return adjT * invDet;
}

float3x3 inverseTranspose3x3(float4x4 M)
{
    float3x3 A = (float3x3)M;
    return transpose(inverse3x3(A));
}