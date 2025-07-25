#pragma once

#include "stdafx.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12DESCRIPTORHEAP
MAIN WORK:
1. create DescriptorHeap
IMPORTANT MEMBER:
1. m_descHeap
*/
class DX12DescriptorHeap
{
public:
	DX12DescriptorHeap();
	~DX12DescriptorHeap();

	void Initialize(
		ID3D12Device* m_device,
		INT numDesc,
		D3D12_DESCRIPTOR_HEAP_TYPE descHeapType,
		D3D12_DESCRIPTOR_HEAP_FLAGS descFlag,
		UINT nodeMask = 0);

	inline ID3D12DescriptorHeap* GetDescHeap() const noexcept { return m_descHeap.Get(); }
	inline UINT GetDescSize() const noexcept { return m_descriptorSize; }
private:
	ComPtr<ID3D12DescriptorHeap> m_descHeap;
	UINT m_descriptorSize = 0;
};