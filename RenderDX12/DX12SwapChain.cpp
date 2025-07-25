#include "stdafx.h"
#include "DX12SwapChain.h"

DX12SwapChain::DX12SwapChain()
{

}

DX12SwapChain::~DX12SwapChain()
{

}

void DX12SwapChain::IntializeMultiSample(ID3D12Device* m_device)
{
	//set anti-aliasing
//set x8 msaa
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS m_sQualityLevels;
	m_sQualityLevels.Format = m_backBufferFormat;
	m_sQualityLevels.SampleCount = 8;
	m_sQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	m_sQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(m_device->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&m_sQualityLevels,
		sizeof(m_sQualityLevels)
	));
	m_8xMsaaQuality = m_sQualityLevels.NumQualityLevels;
	assert(m_8xMsaaQuality > 0 && "Unexpected MSAA quality level.");
}

void DX12SwapChain::InitializeSwapChain(DX12CommandList* m_DX12CommandList, IDXGIFactory4* m_factory, HWND hWnd)
{
	//create swap chain
	m_swapChain.Reset();

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = m_clientWidth;
	swapChainDesc.Height = m_clientHeight;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.Format = m_backBufferFormat;
	swapChainDesc.SampleDesc.Count = m_8xMsaaState ? 8 : 1;
	swapChainDesc.SampleDesc.Quality = m_8xMsaaState ? (m_8xMsaaQuality - 1) : 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = m_swapChainBufferCount;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ComPtr<IDXGISwapChain1> tmpSwapChain;
	ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
		m_DX12CommandList->GetCommandQueue(),
		hWnd, //window
		&swapChainDesc,
		nullptr,
		nullptr,
		&tmpSwapChain
	));
	ThrowIfFailed(tmpSwapChain.As(&m_swapChain));
}