#pragma once
#include "stdafx.h"
#include "DX12Device.h"
#include "DX12Resource.h"
#include "DX12View.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12FRAMEBUFFER
MAIN WORK:
1. command For Frame Drawing
2. create And Manage Components for Drawing frame
3. communicate with RenderDX12 directly for Drawing
IMPORTANT MEMBER:
1. m_renderTargets
2. m_depthStencil
*/
class DX12FrameBuffer
{
public:
    DX12FrameBuffer();
    ~DX12FrameBuffer();


    void Initialize(DX12Device* m_DX12Device);
    void Resize(DX12Device* m_DX12Device);
    void BeginFrame(DX12Device* m_DX12Device);
    void EndFrame(DX12Device* m_DX12Device);
    void Present(DX12Device* m_DX12Device);

private:
    UINT m_currBackBufferIndex = 0;
    std::vector<std::unique_ptr<DX12Resource>> m_DX12RenderTargets;
    std::vector<std::unique_ptr<DX12View>> m_DX12RenderTargetsView;
    std::unique_ptr<DX12ResourceTexture> m_DX12DepthStencil;
    std::unique_ptr<DX12View> m_DX12DepthStencilView;

    std::unique_ptr<DX12ResourceTexture> m_DX12MsaaRenderTarget;
    std::unique_ptr<DX12View> m_DX12MsaaRenderTargetView;
    std::unique_ptr<DX12ResourceTexture> m_DX12MsaaDepthStencil;
    std::unique_ptr<DX12View> m_DX12MsaaDepthStencilView;

    D3D12_CPU_DESCRIPTOR_HANDLE msaaDSVOffsetHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE msaaRTVOffsetHandle = {};

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT     m_scissor{};
};