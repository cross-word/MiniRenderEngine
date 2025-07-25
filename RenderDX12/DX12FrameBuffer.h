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
    std::unique_ptr<DX12ResourceTexture> m_DX12DepthStencil;

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT     m_scissor{};
    DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
};