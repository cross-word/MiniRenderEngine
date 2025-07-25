#pragma once

#include "stdafx.h"
#include "DX12CommandList.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12SWAPCHAIN
MAIN WORK:
1. create SwapChain
IMPORTANT MEMBER:
1. m_swapChain
*/
class DX12SwapChain
{
public:
	DX12SwapChain();
	~DX12SwapChain();

	void IntializeMultiSample(ID3D12Device* m_device);
	void InitializeSwapChain(DX12CommandList* m_DX12CommandList, IDXGIFactory4* m_factory, HWND hWnd);
	inline IDXGISwapChain3* GetSwapChain() const noexcept { return m_swapChain.Get(); }
	inline UINT GetSwapChainBufferCount() const noexcept  { return m_swapChainBufferCount; }
	inline int GetClientWidth() const noexcept { return m_clientWidth; }
	inline int GetClientHeight() const noexcept { return m_clientHeight; }
	inline bool GetMsaaState() const noexcept { return m_8xMsaaQuality; }
	inline UINT GetMsaaQuality() const noexcept { return m_8xMsaaQuality; }
private:
	static const UINT m_swapChainBufferCount = 2;
	ComPtr<IDXGISwapChain3> m_swapChain;

	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	UINT m_clientWidth = 800;
	UINT m_clientHeight = 600;
	// Set true to use 8X MSAA (?.1.8).  The default is false.
	bool      m_8xMsaaState = false;    // 8X MSAA enabled
	UINT      m_8xMsaaQuality = 0;      // quality level of 8X MSAA
};