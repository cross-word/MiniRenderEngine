#include "stdafx.h"
#include "DX12FrameBuffer.h"

DX12FrameBuffer::DX12FrameBuffer()
{

}

DX12FrameBuffer::~DX12FrameBuffer()
{

}

void DX12FrameBuffer::Initialize(DX12Device* m_DX12Device)
{
	assert(m_DX12Device);
	m_DX12DepthStencil = std::make_unique<DX12ResourceTexture>();
	for (int i = 0; i < m_DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); ++i)
	{
		m_DX12RenderTargets.push_back(std::make_unique<DX12Resource>());
	}
	m_DX12MsaaDepthStencil = std::make_unique<DX12ResourceTexture>();
	m_DX12MsaaRenderTarget = std::make_unique<DX12ResourceTexture>();
}

void DX12FrameBuffer::Resize(DX12Device* m_DX12Device)
{
	// Release the previous resources we will be recreating.
	for (int i = 0; i < m_DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); ++i)
		if (m_DX12RenderTargets[i]->GetResource() != nullptr) m_DX12RenderTargets[i]->Reset();
	if (m_DX12DepthStencil->GetResource() != nullptr) m_DX12DepthStencil->Reset();
	if (m_DX12MsaaDepthStencil->GetResource() != nullptr) m_DX12MsaaDepthStencil->Reset(); msaaDSVOffsetHandle = {};
	if (m_DX12MsaaRenderTarget->GetResource() != nullptr) m_DX12MsaaRenderTarget->Reset(); msaaRTVOffsetHandle = {};

	// Resize the swap chain.
	ThrowIfFailed(m_DX12Device->GetDX12SwapChain()->GetSwapChain()->ResizeBuffers(
		m_DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(),
		m_DX12Device->GetDX12SwapChain()->GetClientWidth(),
		m_DX12Device->GetDX12SwapChain()->GetClientHeight(),
		m_DX12Device->GetDX12SwapChain()->GetRenderTargetFormat(),
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	m_currBackBufferIndex = 0;

	//create RTV
	CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHeapHandle(
		m_DX12Device->GetDX12RTVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart()
	);
	for (UINT i = 0; i < m_DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); i++)
	{
		ThrowIfFailed(m_DX12Device->GetDX12SwapChain()->GetSwapChain()->GetBuffer(i, IID_PPV_ARGS(m_DX12RenderTargets[i]->GetAddressOf())));
		m_DX12RenderTargets[i]->TransitionState(m_DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_PRESENT);
		RTVHeapHandle.Offset(1, m_DX12Device->GetDX12RTVHeap()->GetDescSize());
		m_DX12RenderTargetsView.push_back(std::make_unique<DX12View>(
			m_DX12Device->GetDevice(),
			EViewType::ERenderTargetView,
			m_DX12RenderTargets[i].get(),
			RTVHeapHandle
			));
	}
	
	//create DSV
	m_DX12DepthStencil->CreateDepthStencil(
		m_DX12Device->GetDevice(),
		m_DX12Device->GetDX12SwapChain()->GetClientWidth(),
		m_DX12Device->GetDX12SwapChain()->GetClientHeight(),
		m_DX12Device->GetDX12SwapChain()->GetMsaaState() ? 8 : 1,
		m_DX12Device->GetDX12SwapChain()->GetDepthStencilFormat()
	);
	CD3DX12_CPU_DESCRIPTOR_HANDLE DSVHeapHandle(
		m_DX12Device->GetDX12DSVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart()
	);
	m_DX12DepthStencilView = std::make_unique<DX12View>(
		m_DX12Device->GetDevice(),
		EViewType::EDepthStencilView,
		m_DX12DepthStencil.get(),
		DSVHeapHandle
	);
	m_DX12DepthStencil->TransitionState(m_DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
	
	//////////////////////////////////////////////////////////////////////
	//create MSAA RTV
	m_DX12MsaaRenderTarget->CreateRenderTarget(
		m_DX12Device->GetDevice(),
		m_DX12Device->GetDX12SwapChain()->GetClientWidth(),
		m_DX12Device->GetDX12SwapChain()->GetClientHeight(),
		m_DX12Device->GetDX12SwapChain()->GetMsaaState() ? 8 : 1,
		m_DX12Device->GetDX12SwapChain()->GetRenderTargetFormat()
	);
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = m_DX12Device->GetDX12SwapChain()->GetRenderTargetFormat();
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS; // MSAA!
	msaaRTVOffsetHandle = m_DX12Device->GetOffsetCPUHandle(
		m_DX12Device->GetDX12RTVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
		m_DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(),
		m_DX12Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
	);
	
	m_DX12MsaaRenderTargetView = std::make_unique<DX12View>(
		m_DX12Device->GetDevice(),
		EViewType::ERenderTargetView,
		m_DX12MsaaRenderTarget.get(),
		msaaRTVOffsetHandle
	);

	//create MSAA DSV
	m_DX12MsaaDepthStencil->CreateDepthStencil(
		m_DX12Device->GetDevice(),
		m_DX12Device->GetDX12SwapChain()->GetClientWidth(),
		m_DX12Device->GetDX12SwapChain()->GetClientHeight(),
		m_DX12Device->GetDX12SwapChain()->GetMsaaState() ? 8 : 1,
		m_DX12Device->GetDX12SwapChain()->GetDepthStencilFormat()
	);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = m_DX12Device->GetDX12SwapChain()->GetDepthStencilFormat();
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS; // MSAA!

	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	msaaDSVOffsetHandle = m_DX12Device->GetOffsetCPUHandle(
		m_DX12Device->GetDX12DSVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
		1,
		m_DX12Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
	);

	m_DX12MsaaDepthStencilView = std::make_unique<DX12View>(
		m_DX12Device->GetDevice(),
		EViewType::EDepthStencilView,
		m_DX12MsaaDepthStencil.get(),
		msaaDSVOffsetHandle
	);
	m_DX12MsaaDepthStencil->TransitionState(m_DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
	//////////////////////////////////////////////////////////////////////
	
	// Update the viewport transform to cover the client area.
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
	m_viewport.Width = static_cast<float>(m_DX12Device->GetDX12SwapChain()->GetClientWidth());
	m_viewport.Height = static_cast<float>(m_DX12Device->GetDX12SwapChain()->GetClientHeight());
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_scissor = { 0, 0, m_DX12Device->GetDX12SwapChain()->GetClientWidth(), m_DX12Device->GetDX12SwapChain()->GetClientHeight() };

	// Execute the resize commands.
	m_DX12Device->GetDX12CommandList()->SubmitAndWait();
}

void DX12FrameBuffer::BeginFrame(DX12Device* m_DX12Device)
{
	m_DX12Device->GetDX12CommandList()->GetCommandList()->RSSetViewports(1, &m_viewport);
	m_DX12Device->GetDX12CommandList()->GetCommandList()->RSSetScissorRects(1, &m_scissor);

	float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
	m_DX12Device->GetDX12CommandList()->GetCommandList()->OMSetRenderTargets(1, &msaaRTVOffsetHandle, FALSE, &msaaDSVOffsetHandle);
	m_DX12Device->GetDX12CommandList()->GetCommandList()->ClearRenderTargetView(msaaRTVOffsetHandle, clearColor, 0, nullptr);
	m_DX12Device->GetDX12CommandList()->GetCommandList()->ClearDepthStencilView(msaaDSVOffsetHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
}

void DX12FrameBuffer::EndFrame(DX12Device* m_DX12Device)
{
	//MSAA RESOLVE
	//MsaaRTv : RT->ResolveSource
	//BackBuffer : RT->ResolveDest
	m_DX12MsaaRenderTarget->TransitionState(m_DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
	m_DX12RenderTargets[m_currBackBufferIndex]->TransitionState(m_DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_RESOLVE_DEST);
	
	m_DX12Device->GetDX12CommandList()->GetCommandList()->ResolveSubresource(
		m_DX12RenderTargets[m_currBackBufferIndex]->GetResource(),
		0,
		m_DX12MsaaRenderTarget->GetResource(),
		0,
		m_DX12Device->GetDX12SwapChain()->GetRenderTargetFormat());
	
	//MsaaRTv : ResolveSource->RT
	//BackBuffer : ResolveDest->PRESENT
	m_DX12MsaaRenderTarget->TransitionState(m_DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_DX12RenderTargets[m_currBackBufferIndex]->TransitionState(m_DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_PRESENT);
	m_DX12Device->GetDX12CommandList()->SubmitAndWait();
}

void DX12FrameBuffer::Present(DX12Device* m_DX12Device)
{
	ThrowIfFailed(m_DX12Device->GetDX12SwapChain()->GetSwapChain()->Present(1, 0));
	m_currBackBufferIndex = (m_currBackBufferIndex + 1) % m_DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount();
}