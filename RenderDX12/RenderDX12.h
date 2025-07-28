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

#include "DX12device.h"
#include "DX12FrameBuffer.h"
#include "D3DCamera.h"
/*
CLASS RENDERDX12
MAIN WORK:
1. initialize dx12Device, dx12FrameBuffer (start point of DX12 RENDERING)
2. comunicate with MainWindow
IMPORTANT MEMBER:
1. m_DX12Device
2. m_FrameBuffer

Variable Naming Rule in RenderDX12 project:
1. d3d12 API variable && member variable -> m_~
2. custom class DX12 valibale && member variable -> m_DX12~
*/
class RenderDX12_API RenderDX12
{
public:
    RenderDX12();
    ~RenderDX12();

public:
    void InitializeDX12(HWND hWnd);
    void OnResize();
    void Draw();
    D3DCamera* GetD3DCamera() const noexcept { return m_DX12Device.GetD3DCamera(); }
private:
    ComPtr<ID3D12Debug> m_debugController;
    DX12Device m_DX12Device;
    DX12FrameBuffer m_DX12FrameBuffer;
};