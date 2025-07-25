#include "stdafx.h"
#include "RenderDX12.h"

#include <d3d12sdklayers.h>

RenderDX12::RenderDX12()
{

}

RenderDX12::~RenderDX12()
{
	
}

void RenderDX12::InitializeDX12(HWND hWnd)
{
	assert(hWnd);
	UINT dxgiFactoryFlags = 0;

//enable debug layer
#if defined(DEBUG) || defined(_DEBUG)
{
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController))))
	{
		m_debugController->EnableDebugLayer();

		// Enable additional debug layers.
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
}
#endif
	m_DX12Device.Initialize(hWnd);
	m_DX12FrameBuffer.Initialize(&m_DX12Device);
	OnResize();
	m_DX12Device.PrepareInitialResource();
}

void RenderDX12::OnResize()
{
	assert(m_DX12Device.GetDevice());
	assert(m_DX12Device.GetDX12SwapChain());
	assert(m_DX12Device.GetDX12CommandList()->GetCommandAllocator());

	// Flush before changing any resources.
	m_DX12Device.GetDX12CommandList()->FlushCommandQueue();
	m_DX12Device.GetDX12CommandList()->ResetList();

	m_DX12FrameBuffer.Resize(&m_DX12Device);

	// Wait until resize is complete.
	m_DX12Device.GetDX12CommandList()->FlushCommandQueue();
}

void RenderDX12::Draw()
{
	m_DX12Device.GetDX12CommandList()->ResetAllocator();
	m_DX12Device.GetDX12CommandList()->ResetList(m_DX12Device.GetDX12PSO()->GetPipelineState()); //tmpcode

	m_DX12Device.UpdateConstantBuffer();
	m_DX12FrameBuffer.BeginFrame(&m_DX12Device);

	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootSignature(m_DX12Device.GetDX12RootSignature()->GetRootSignature());
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_DX12Device.GetDX12CBVHeap()->GetDescHeap()};
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootDescriptorTable(0, m_DX12Device.GetDX12CBVHeap()->GetDescHeap()->GetGPUDescriptorHandleForHeapStart());

	m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetVertexBuffers(0, 1, m_DX12Device.GetDX12VertexBufferView()->GetVertexBufferView());
	m_DX12Device.GetDX12CommandList()->GetCommandList()->DrawInstanced(3, 1, 0, 0);

	m_DX12FrameBuffer.EndFrame(&m_DX12Device);

	m_DX12FrameBuffer.Present(&m_DX12Device);
	m_DX12Device.GetDX12CommandList()->FlushCommandQueue();
}