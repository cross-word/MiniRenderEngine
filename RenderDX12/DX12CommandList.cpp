#include "stdafx.h"
#include "DX12CommandList.h"


DX12CommandList::DX12CommandList()
{

}

DX12CommandList::~DX12CommandList()
{
	if (m_fenceEvent) CloseHandle(m_fenceEvent);
}

void DX12CommandList::Initialize(ID3D12Device* device, ID3D12CommandAllocator* commandAllocator)
{
	//create command queue/allocator
	D3D12_COMMAND_QUEUE_DESC m_queueDesc = {};
	m_queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	m_queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	ThrowIfFailed(device->CreateCommandQueue(
		&m_queueDesc, IID_PPV_ARGS(&m_commandQueue)
	));
	ThrowIfFailed(device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator,
		nullptr,
		IID_PPV_ARGS(m_commandList.GetAddressOf())
	));

	m_commandList->Close();

	//create fence
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

	return;
}

void DX12CommandList::FlushCommandQueue(UINT currentFenceValue)
{
	// Advance the fence value to mark commands up to this fence point.
	currentFenceValue++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	// Wait until the GPU has completed commands up to this fence point.
	if (m_fence->GetCompletedValue() < currentFenceValue)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(m_fence->SetEventOnCompletion(currentFenceValue, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

//void DX12CommandList::ResetAllocator()
//{
//	ThrowIfFailed(m_commandAllocator->Reset());
//	return;
//}

void DX12CommandList::ResetList(ID3D12CommandAllocator* commandAllocator)
{
	ThrowIfFailed(m_commandList->Reset(commandAllocator, nullptr));
	return;
}

void DX12CommandList::ResetList(ID3D12PipelineState* pInitiaState, ID3D12CommandAllocator* commandAllocator)
{
	ThrowIfFailed(m_commandList->Reset(commandAllocator, pInitiaState));
	return;
}

void DX12CommandList::ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList** ppCommandLists)
{
	m_commandQueue->ExecuteCommandLists(NumCommandLists, ppCommandLists);

	return;
}

void DX12CommandList::SubmitAndWait(UINT currentFenceValue)
{
	m_commandList->Close();
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get()};
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	FlushCommandQueue(currentFenceValue);
}