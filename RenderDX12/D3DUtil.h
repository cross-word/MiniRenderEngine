#pragma once

#include <string>
#include <wrl.h>
#include <DirectXMath.h>
#include "../MiniEngineCore/EngineConfig.h"
#include "../FileLoader/GLTFLoader.h"
using namespace DirectX;

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString()const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

enum class EViewType
{
    EVertexView,
    EIndexView,
    EConstantBufferView,
    EShaderResourceView,
    EUnorderedAccessView,
    EDepthStencilView,
    ERenderTargetView
};

#define MaxLights 25
static XMFLOAT4X4 XMMatIdentity()
{
    return XMFLOAT4X4({ 1, 0, 0, 0,
                        0, 1, 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 1});
}

struct PassConstants // to slot b0 (per camera)
{
    XMFLOAT4X4 View = XMMatIdentity();
    XMFLOAT4X4 InvView = XMMatIdentity();
    XMFLOAT4X4 Proj = XMMatIdentity();
    XMFLOAT4X4 InvProj = XMMatIdentity();
    XMFLOAT4X4 ViewProj = XMMatIdentity();
    XMFLOAT4X4 InvViewProj = XMMatIdentity();
    XMFLOAT3 EyePosW = XMFLOAT3{ 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    XMFLOAT2 RenderTargetSize = XMFLOAT2{ 0.0f, 0.0f };
    XMFLOAT2 InvRenderTargetSize = XMFLOAT2{ 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;

    XMFLOAT4 AmbientLight = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light Lights[MaxLights];
};

struct ObjectConstants
{
    XMFLOAT4X4 World = XMMatIdentity();
    XMFLOAT4X4 TexTransform = XMMatIdentity();
};

struct MaterialConstants
{
    XMFLOAT4 DiffuseAlbedo = { 1,1,1,1 }; // = baseColorFactor
    XMFLOAT3 FresnelR0 = { 0.04f,0.04f,0.04f };
    float              Roughness = 1.0f;      // perceptual roughness [0..1]

    XMFLOAT4X4 MatTransform = XMMatIdentity(); // baseColor

    float Metallic = 1.0f;        // metallicFactor
    float NormalScale = 1.0f;        // normalTexture.scale
    float OcclusionStrength = 1.0f;        // occlusionTexture.strength
    float EmissiveStrength = 1.0f;        // KHR_materials_emissive_strength

    XMFLOAT3 EmissiveFactor = { 0,0,0 };
    //float _pad0 = 0.f;

    uint32_t BaseColorIndex = UINT32_MAX;
    uint32_t NormalIndex = UINT32_MAX; 
    uint32_t ORMIndex = UINT32_MAX;
    uint32_t OcclusionIndex = UINT32_MAX;
    uint32_t EmissiveIndex = UINT32_MAX;
};
static_assert(sizeof(MaterialConstants) % 16 == 0, "CB/SSBO align");

struct Material // to slot b2 (per object)
{
    // Unique material name for lookup.
    std::string Name;

    // Index into constant buffer corresponding to this material.
    int MatCBIndex = -1;

    // Index into SRV heap for diffuse texture.
    int DiffuseSrvHeapIndex = -1;

    // Index into SRV heap for normal texture.
    int NormalSrvHeapIndex = -1;

    // Dirty flag indicating the material has changed and we need to update the constant buffer.
    // Because we have a material constant buffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify a material we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = EngineConfig::SwapChainBufferCount;

    MaterialConstants matConstant;
};

static UINT CalcConstantBufferByteSize(UINT byteSize)
{
    // Constant buffers must be a multiple of the minimum hardware
    // allocation size (usually 256 bytes).  So round up to nearest
    // multiple of 256.  We do this by adding 255 and then masking off
    // the lower 2 bytes which store all bits < 256.
    // Example: Suppose byteSize = 300.
    // (300 + 255) & ~255
    // 555 & ~255
    // 0x022B & ~0x00ff
    // 0x022B & 0xff00
    // 0x0200
    // 512
    return (byteSize + 255) & ~255;
}

struct HeapSlice {
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle{};
};