#include "stdafx.h"
#include "DX12DescriptorHeap.h"

DX12DescriptorHeap::DX12DescriptorHeap()
{

}

DX12DescriptorHeap::~DX12DescriptorHeap()
{

}

void DX12DescriptorHeap::Initialize(
	ID3D12Device* m_device,
	INT numDesc,
	D3D12_DESCRIPTOR_HEAP_TYPE descHeapType,
	D3D12_DESCRIPTOR_HEAP_FLAGS descFlag,
	UINT nodeMask)
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc{};
	cbvHeapDesc.NumDescriptors = numDesc;
	cbvHeapDesc.Type = descHeapType;
	cbvHeapDesc.Flags = descFlag;
	cbvHeapDesc.NodeMask = nodeMask;
	ThrowIfFailed(m_device->CreateDescriptorHeap(
		&cbvHeapDesc, IID_PPV_ARGS(m_descHeap.GetAddressOf())
	));
	m_descriptorSize = m_device->GetDescriptorHandleIncrementSize(descHeapType);
}