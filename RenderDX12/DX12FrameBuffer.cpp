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
		m_DX12RenderTargets.push_back(std::make_unique<DX12Resource>());
}

void DX12FrameBuffer::Resize(DX12Device* m_DX12Device)
{
	// Release the previous resources we will be recreating.
	for (int i = 0; i < m_DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); ++i)
		if (m_DX12RenderTargets[i]->GetResource() != nullptr) m_DX12RenderTargets[i]->Reset();
	if(m_DX12DepthStencil->GetResource() != nullptr) m_DX12DepthStencil->Reset();

	// Resize the swap chain.
	ThrowIfFailed(m_DX12Device->GetDX12SwapChain()->GetSwapChain()->ResizeBuffers(
		m_DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(),
		m_DX12Device->GetDX12SwapChain()->GetClientWidth(),
		m_DX12Device->GetDX12SwapChain()->GetClientHeight(),
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	m_currBackBufferIndex = 0;

	//create RTV
	CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHeapHandle(
		m_DX12Device->GetDX12RTVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart()
	);
	for (UINT i = 0; i < m_DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); i++)
	{
		ThrowIfFailed(m_DX12Device->GetDX12SwapChain()->GetSwapChain()->GetBuffer(i, IID_PPV_ARGS(m_DX12RenderTargets[i]->GetAddressOf())));
		m_DX12Device->GetDevice()->CreateRenderTargetView(m_DX12RenderTargets[i]->GetResource(), nullptr, RTVHeapHandle);
		RTVHeapHandle.Offset(1, m_DX12Device->GetDX12RTVHeap()->GetDescSize());
	}
	
	m_DX12DepthStencil->CreateDepthStencil(
		m_DX12Device->GetDevice(),
		m_DX12Device->GetDX12SwapChain()->GetClientWidth(),
		m_DX12Device->GetDX12SwapChain()->GetClientHeight(),
		m_DX12Device->GetDX12SwapChain()->GetMsaaState() ? 8 : 1,
		m_DX12Device->GetDX12SwapChain()->GetMsaaState() ? (m_DX12Device->GetDX12SwapChain()->GetMsaaQuality() - 1) : 0,
		m_depthStencilFormat
	);

	m_DX12Device->GetDevice()->CreateDepthStencilView(
		m_DX12DepthStencil->GetResource(),
		nullptr,
		m_DX12Device->GetDX12DSVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart()
	);

	m_DX12DepthStencil->TransitionState(m_DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// Update the viewport transform to cover the client area.
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
	m_viewport.Width = static_cast<float>(m_DX12Device->GetDX12SwapChain()->GetClientWidth());
	m_viewport.Height = static_cast<float>(m_DX12Device->GetDX12SwapChain()->GetClientHeight());
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_scissor = { 0, 0, m_DX12Device->GetDX12SwapChain()->GetClientWidth(), m_DX12Device->GetDX12SwapChain()->GetClientHeight() };

	// Execute the resize commands.
	ThrowIfFailed(m_DX12Device->GetDX12CommandList()->GetCommandList()->Close());
	ID3D12CommandList* cmdsLists[] = { m_DX12Device->GetDX12CommandList()->GetCommandList() };
	m_DX12Device->GetDX12CommandList()->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
}

void DX12FrameBuffer::BeginFrame(DX12Device* m_DX12Device)
{
	m_DX12Device->GetDX12CommandList()->GetCommandList()->RSSetViewports(1, &m_viewport);
	m_DX12Device->GetDX12CommandList()->GetCommandList()->RSSetScissorRects(1, &m_scissor);

	//PRESENT -> RT
	m_DX12RenderTargets[m_currBackBufferIndex]->TransitionState(m_DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	auto rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_DX12Device->GetDX12RTVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
		m_currBackBufferIndex, m_DX12Device->GetDX12RTVHeap()->GetDescSize()
	);
	float clearColor[] = { 0.1f, 0.1f, 0.3f, 1.0f };
	m_DX12Device->GetDX12CommandList()->GetCommandList()->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_DX12Device->GetDX12CommandList()->GetCommandList()->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
}

void DX12FrameBuffer::EndFrame(DX12Device* m_DX12Device)
{
	//RT -> PRESENT
	m_DX12RenderTargets[m_currBackBufferIndex]->TransitionState(m_DX12Device->GetDX12CommandList()->GetCommandList(), D3D12_RESOURCE_STATE_PRESENT);

	ThrowIfFailed(m_DX12Device->GetDX12CommandList()->GetCommandList()->Close());
	ID3D12CommandList* cmdsLists[] = { m_DX12Device->GetDX12CommandList()->GetCommandList() };
	m_DX12Device->GetDX12CommandList()->GetCommandQueue()->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
}

void DX12FrameBuffer::Present(DX12Device* m_DX12Device)
{
	ThrowIfFailed(m_DX12Device->GetDX12SwapChain()->GetSwapChain()->Present(1, 0));
	m_currBackBufferIndex = (m_currBackBufferIndex + 1) % m_DX12Device->GetDX12SwapChain()->GetSwapChainBufferCount();
}