#include "stdafx.h"
#include "DX12FrameBuffer.h"
#include "../MiniEngineCore/EngineConfig.h"

DX12FrameBuffer::DX12FrameBuffer()
{

}

DX12FrameBuffer::~DX12FrameBuffer()
{

}

void DX12FrameBuffer::Initialize(DX12Device* DX12Device)
{
	assert(DX12Device);
	m_DX12DepthStencil = std::make_unique<DX12ResourceTexture>();
	for (int i = 0; i < DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); ++i)
	{
		m_DX12RenderTargets.push_back(std::make_unique<DX12Resource>());
	}
	m_DX12MsaaDepthStencil = std::make_unique<DX12ResourceTexture>();
	m_DX12MsaaRenderTarget = std::make_unique<DX12ResourceTexture>();
}

void DX12FrameBuffer::Resize(DX12Device* DX12Device)
{
	// Release the previous resources we will be recreating.
	for (int i = 0; i < DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); ++i)
		if (m_DX12RenderTargets[i]->GetResource() != nullptr) m_DX12RenderTargets[i]->Reset();
	if (m_DX12DepthStencil->GetResource() != nullptr) m_DX12DepthStencil->Reset();
	if (m_DX12MsaaDepthStencil->GetResource() != nullptr) m_DX12MsaaDepthStencil->Reset(); msaaDSVOffsetHandle = {};
	if (m_DX12MsaaRenderTarget->GetResource() != nullptr) m_DX12MsaaRenderTarget->Reset(); msaaRTVOffsetHandle = {};

	// Resize the swap chain.
	ThrowIfFailed(DX12Device->GetDX12SwapChain()->GetSwapChain()->ResizeBuffers(
		DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(),
		DX12Device->GetDX12SwapChain()->GetClientWidth(),
		DX12Device->GetDX12SwapChain()->GetClientHeight(),
		DX12Device->GetDX12SwapChain()->GetRenderTargetFormat(),
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	m_currBackBufferIndex = 0;

	CreateRenderTargetsAndViews(DX12Device);
	CreateDepthStencilAndView(DX12Device);
	CreateMsaaRenderTargetAndView(DX12Device);
	CreateMsaaDepthStencilAndView(DX12Device);
	SetViewPortAndScissor(DX12Device);

	// Execute the resize commands.
	DX12Device->GetDX12CommandList()->SubmitAndWait();
}

void DX12FrameBuffer::CreateRenderTargetsAndViews(DX12Device* DX12Device)
{
	//create RTV
	CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHeapHandle(
		DX12Device->GetDX12RTVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart()
	);
	for (UINT i = 0; i < DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); i++)
	{
		ThrowIfFailed(DX12Device->GetDX12SwapChain()->GetSwapChain()->GetBuffer(i, IID_PPV_ARGS(m_DX12RenderTargets[i]->GetAddressOf())));
		m_DX12RenderTargets[i]->TransitionState(DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_PRESENT);
		RTVHeapHandle.Offset(1, DX12Device->GetDX12RTVHeap()->GetDescSize());
		m_DX12RenderTargetViews.push_back(std::make_unique<DX12View>(
			DX12Device->GetDevice(),
			EViewType::ERenderTargetView,
			m_DX12RenderTargets[i].get(),
			RTVHeapHandle
		));
	}
}

void DX12FrameBuffer::CreateDepthStencilAndView(DX12Device* DX12Device)
{
	//create DSV
	m_DX12DepthStencil->CreateDepthStencil(
		DX12Device->GetDevice(),
		DX12Device->GetDX12SwapChain()->GetClientWidth(),
		DX12Device->GetDX12SwapChain()->GetClientHeight(),
		DX12Device->GetDX12SwapChain()->GetMsaaSampleCount(),
		DX12Device->GetDX12SwapChain()->GetDepthStencilFormat()
	);
	CD3DX12_CPU_DESCRIPTOR_HANDLE DSVHeapHandle(
		DX12Device->GetDX12DSVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart()
	);
	m_DX12DepthStencilView = std::make_unique<DX12View>(
		DX12Device->GetDevice(),
		EViewType::EDepthStencilView,
		m_DX12DepthStencil.get(),
		DSVHeapHandle
	);
	m_DX12DepthStencil->TransitionState(DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void DX12FrameBuffer::CreateMsaaRenderTargetAndView(DX12Device* DX12Device)
{
	//create MSAA RTV
	m_DX12MsaaRenderTarget->CreateRenderTarget(
		DX12Device->GetDevice(),
		DX12Device->GetDX12SwapChain()->GetClientWidth(),
		DX12Device->GetDX12SwapChain()->GetClientHeight(),
		DX12Device->GetDX12SwapChain()->GetMsaaSampleCount(),
		DX12Device->GetDX12SwapChain()->GetRenderTargetFormat()
	);
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DX12Device->GetDX12SwapChain()->GetRenderTargetFormat();
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS; // MSAA!
	msaaRTVOffsetHandle = DX12Device->GetOffsetCPUHandle(
		DX12Device->GetDX12RTVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
		DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(),
		DX12Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
	);

	m_DX12MsaaRenderTargetView = std::make_unique<DX12View>(
		DX12Device->GetDevice(),
		EViewType::ERenderTargetView,
		m_DX12MsaaRenderTarget.get(),
		msaaRTVOffsetHandle
	);
}

void DX12FrameBuffer::CreateMsaaDepthStencilAndView(DX12Device* DX12Device)
{
	//create MSAA DSV
	m_DX12MsaaDepthStencil->CreateDepthStencil(
		DX12Device->GetDevice(),
		DX12Device->GetDX12SwapChain()->GetClientWidth(),
		DX12Device->GetDX12SwapChain()->GetClientHeight(),
		DX12Device->GetDX12SwapChain()->GetMsaaSampleCount(),
		DX12Device->GetDX12SwapChain()->GetDepthStencilFormat()
	);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DX12Device->GetDX12SwapChain()->GetDepthStencilFormat();
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS; // MSAA!

	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	msaaDSVOffsetHandle = DX12Device->GetOffsetCPUHandle(
		DX12Device->GetDX12DSVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
		1,
		DX12Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
	);

	m_DX12MsaaDepthStencilView = std::make_unique<DX12View>(
		DX12Device->GetDevice(),
		EViewType::EDepthStencilView,
		m_DX12MsaaDepthStencil.get(),
		msaaDSVOffsetHandle
	);
	m_DX12MsaaDepthStencil->TransitionState(DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void DX12FrameBuffer::SetViewPortAndScissor(DX12Device* DX12Device)
{
	// Update the viewport transform to cover the client area.
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
	m_viewport.Width = static_cast<float>(DX12Device->GetDX12SwapChain()->GetClientWidth());
	m_viewport.Height = static_cast<float>(DX12Device->GetDX12SwapChain()->GetClientHeight());
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_scissor = { 0, 0, DX12Device->GetDX12SwapChain()->GetClientWidth(), DX12Device->GetDX12SwapChain()->GetClientHeight() };
}

void DX12FrameBuffer::BeginFrame(DX12Device* DX12Device)
{
	DX12Device->GetDX12CommandList()->GetCommandList()->RSSetViewports(1, &m_viewport);
	DX12Device->GetDX12CommandList()->GetCommandList()->RSSetScissorRects(1, &m_scissor);

	DX12Device->GetDX12CommandList()->GetCommandList()->OMSetRenderTargets(1, &msaaRTVOffsetHandle, FALSE, &msaaDSVOffsetHandle);
	DX12Device->GetDX12CommandList()->GetCommandList()->ClearRenderTargetView(msaaRTVOffsetHandle, EngineConfig::DefaultClearColor, 0, nullptr);
	DX12Device->GetDX12CommandList()->GetCommandList()->ClearDepthStencilView(msaaDSVOffsetHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
}

void DX12FrameBuffer::EndFrame(DX12Device* DX12Device)
{
	//MSAA RESOLVE
	//MsaaRTv : RT->ResolveSource
	//BackBuffer : RT->ResolveDest
	m_DX12MsaaRenderTarget->TransitionState(DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
	m_DX12RenderTargets[m_currBackBufferIndex]->TransitionState(DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_RESOLVE_DEST);
	
	DX12Device->GetDX12CommandList()->GetCommandList()->ResolveSubresource(
		m_DX12RenderTargets[m_currBackBufferIndex]->GetResource(),
		0,
		m_DX12MsaaRenderTarget->GetResource(),
		0,
		DX12Device->GetDX12SwapChain()->GetRenderTargetFormat());
	
	//MsaaRTv : ResolveSource->RT
	//BackBuffer : ResolveDest->PRESENT
	m_DX12MsaaRenderTarget->TransitionState(DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_DX12RenderTargets[m_currBackBufferIndex]->TransitionState(DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_PRESENT);
	DX12Device->GetDX12CommandList()->SubmitAndWait();
}

void DX12FrameBuffer::Present(DX12Device* DX12Device)
{
	ThrowIfFailed(DX12Device->GetDX12SwapChain()->GetSwapChain()->Present(1, 0));
	m_currBackBufferIndex = (m_currBackBufferIndex + 1) % DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount();
}