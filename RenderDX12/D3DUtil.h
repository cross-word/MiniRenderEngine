#pragma once

#include <string>
#include <wrl.h>
#include <DirectXMath.h>
#include "../MiniEngineCore/EngineConfig.h"
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

struct Light
{
    DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
    float FalloffStart = 1.0f;                          // point/spot light only
    DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
    float FalloffEnd = 10.0f;                           // point/spot light only
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
    float SpotPower = 64.0f;                            // spot light only
};


#define MaxLights 16
static XMFLOAT4X4 XMMatIdentity()
{
    return XMFLOAT4X4({ 1, 0, 0, 0,
                        0, 1, 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 1
                                });
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

struct ObjectConstants // to slot b1 (per draw call)
{
    XMMATRIX World = XMMatrixIdentity();
    XMFLOAT4X4 TexTransform = XMMatIdentity();
};


struct MaterialConstants // to slot b2 (per object)
{
    XMFLOAT4 DiffuseAlbedo = XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
    XMFLOAT3 FresnelR0 = XMFLOAT3{ 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;

    // Used in texture mapping.
    XMFLOAT4X4 MatTransform = XMMatIdentity();
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