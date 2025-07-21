#pragma once

#ifdef _BUILDING_RENDERDX12
#define RenderDX12_API __declspec(dllexport)
#else
#define RenderDX12_API __declspec(dllimport)
#endif

#include "stdafx.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#include "D3DUtil.h"

class RenderDX12_API RenderDX12
{
public:
    RenderDX12();
    ~RenderDX12();

private:
    void OnResize();
    void FlushCommandQueue();
    void PrepareRendering();
    void InitRootSignature();
    void InitShader();
    void InitPSO();
    void CreateTriangleVertexBuffer();
    void CreateConstantBuffer();
public:
    void InitializeDX12(HWND hWnd);
    void Draw();

private:
    static const UINT m_swapChainBufferCount = 2;

    ComPtr<ID3D12Debug> debugController;

    D3D12_VIEWPORT m_viewport;
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[m_swapChainBufferCount];
    ComPtr<ID3D12Resource> m_depthStencil;
    ComPtr<ID3D12Resource> m_cUploadBuffer;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12PipelineState> m_pipelineState1;
    ComPtr<ID3D12PipelineState> m_pipelineState2;
    ComPtr<ID3D12Fence> m_fence;
    D3D12_VIEWPORT m_screenViewport;
    D3D12_RECT m_scissorRect;

    UINT m_rtvDescriptorSize = 0;
    UINT m_dsvDescriptorSize = 0;
    UINT m_cbvSrvDescriptorSize = 0;
    UINT m_currentFenceValue = 0;
    UINT m_currentBackBuffer = 0;

    DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    int m_clientWidth = 800;
    int m_clientHeight = 600;
    
    ComPtr<ID3DBlob> m_vertexShader = nullptr;
    ComPtr<ID3DBlob> m_pixelShader = nullptr;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    // Set true to use 8X MSAA (?.1.8).  The default is false.
    bool      m_8xMsaaState = false;    // 8X MSAA enabled
    UINT      m_8xMsaaQuality = 0;      // quality level of 8X MSAA

    float m_aspectRatio = static_cast<float>(m_clientWidth) / m_clientHeight;

    struct ObjectConstants
    {
        DirectX::XMFLOAT4X4 WorldViewProj = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
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
};

struct Vertex
{
    XMFLOAT3 pos;
    XMFLOAT4 color;
};