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
	for(int i = 0; i < EngineConfig::SwapChainBufferCount; i++) assert(m_DX12Device.GetFrameResource(i)->GetCommandAllocator());

	// Flush before changing any resources.
	m_DX12Device.GetDX12CommandList()->FlushCommandQueue();
	m_DX12Device.GetDX12CommandList()->ResetList(m_DX12Device.GetFrameResource(m_DX12Device.GetCurrentBackBufferIndex())->GetCommandAllocator());

	m_DX12FrameBuffer.Resize(&m_DX12Device);
}

void RenderDX12::Draw()
{
	m_DX12Device.SetCurrentBackBufferIndex(m_DX12Device.GetDX12SwapChain()->GetSwapChain()->GetCurrentBackBufferIndex());
	UINT currBackBufferIndex = m_DX12Device.GetDX12SwapChain()->GetSwapChain()->GetCurrentBackBufferIndex();
	m_DX12Device.UpdateFrameResource();

	m_DX12Device.GetFrameResource(currBackBufferIndex)->ResetAllocator();
	m_DX12Device.GetDX12CommandList()->ResetList(m_DX12Device.GetDX12PSO()->GetPipelineState(), m_DX12Device.GetFrameResource(currBackBufferIndex)->GetCommandAllocator()); //tmpcode

	m_DX12FrameBuffer.BeginFrame(&m_DX12Device, currBackBufferIndex);

	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootSignature(m_DX12Device.GetDX12RootSignature()->GetRootSignature());
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_DX12Device.GetDX12CBVHeap()->GetDescHeap()};
	auto descGPUAddress = m_DX12Device.GetDX12CBVHeap()->GetDescHeap()->GetGPUDescriptorHandleForHeapStart();
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootDescriptorTable(0, { descGPUAddress.ptr + 0 * m_DX12Device.GetDX12CBVHeap()->GetDescIncSize() });

	m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetVertexBuffers(0, 1, m_DX12Device.GetDX12VertexBufferView()->GetVertexBufferView());
	m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetIndexBuffer(m_DX12Device.GetDX12IndexBufferView()->GetIndexBufferView());
	m_DX12Device.GetDX12CommandList()->GetCommandList()->DrawIndexedInstanced(36, 1, 0, 0, 0);

	m_DX12FrameBuffer.EndFrame(&m_DX12Device, currBackBufferIndex);

	m_DX12Device.GetDX12CommandList()->GetCommandList()->Close();
	ID3D12CommandList* lists[] = { m_DX12Device.GetDX12CommandList()->GetCommandList() };
	m_DX12Device.GetDX12CommandList()->GetCommandQueue()->ExecuteCommandLists(1, lists);

	// 프레임 펜스 기록(대기는 다음 프레임 Begin에서만)
	const UINT64 fenceValue = ++fenceCounter;
	m_DX12Device.GetDX12CommandList()->GetCommandQueue()->Signal(m_DX12Device.GetDX12CommandList()->GetFence(), fenceValue);
	m_DX12Device.GetFrameResource(currBackBufferIndex)->SetFenceValue(fenceValue);

	m_DX12FrameBuffer.Present(&m_DX12Device);
}

void RenderDX12::ShutDown()
{
	m_DX12Device.GetDX12CommandList()->FlushCommandQueue();
}