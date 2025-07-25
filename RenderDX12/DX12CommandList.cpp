#include "stdafx.h"
#include "DX12CommandList.h"


DX12CommandList::DX12CommandList()
{

}

DX12CommandList::~DX12CommandList()
{
	FlushCommandQueue();
}

void DX12CommandList::Initialize(ID3D12Device* m_device)
{
	//create command queue/allocator
	D3D12_COMMAND_QUEUE_DESC m_queueDesc = {};
	m_queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	m_queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	ThrowIfFailed(m_device->CreateCommandQueue(
		&m_queueDesc, IID_PPV_ARGS(&m_commandQueue)
	));
	ThrowIfFailed(m_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_commandAllocator.GetAddressOf())
	));
	ThrowIfFailed(m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_commandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(m_commandList.GetAddressOf())
	));

	m_commandList->Close();

	//create fence
	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

	return;
}

void DX12CommandList::FlushCommandQueue()
{
	// Advance the fence value to mark commands up to this fence point.
	m_currentFenceValue++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFenceValue));

	// Wait until the GPU has completed commands up to this fence point.
	if (m_fence->GetCompletedValue() < m_currentFenceValue)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFenceValue, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void DX12CommandList::ResetAllocator()
{
	ThrowIfFailed(m_commandAllocator->Reset());
	return;
}

void DX12CommandList::ResetList()
{
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));
	return;
}

void DX12CommandList::ResetList(ID3D12PipelineState* pInitiaState)
{
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), pInitiaState));
	return;
}

void DX12CommandList::ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList** ppCommandLists)
{
	m_commandQueue->ExecuteCommandLists(NumCommandLists, ppCommandLists);

	return;
}