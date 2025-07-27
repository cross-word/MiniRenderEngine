#pragma once

#include "stdafx.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12COMMANDLIST
MAIN WORK:
1. create command List, Allocator, Queue, and fence.
2. manage Fence Value
IMPORTANT MEMBER:
1. m_commandAllocator
2. m_commandList
3. m_commandQueue
4. m_fence
5. FlushCommandQueue()
*/
class DX12CommandList
{
public:
	DX12CommandList();
	~DX12CommandList();
	void Initialize(ID3D12Device* m_device);
	inline ID3D12CommandAllocator* GetCommandAllocator() const noexcept { return m_commandAllocator.Get(); }
	inline ID3D12GraphicsCommandList* GetCommandList() const noexcept { return m_commandList.Get(); }
	inline ID3D12CommandQueue* GetCommandQueue() const noexcept { return m_commandQueue.Get(); }
	inline ID3D12Fence* GetFence() const noexcept { return m_fence.Get(); }

	void FlushCommandQueue();
	void ResetAllocator();
	void ResetList();
	void ResetList(ID3D12PipelineState* pInitiaState);
	void ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList** ppCommandLists);
	void SubmitAndWait();
private:
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12Fence> m_fence;

	UINT m_currentFenceValue = 0;
};