#include "stdafx.h"
#include "DX12FrameResource.h"

DX12FrameResource::DX12FrameResource(ID3D12Device* device, DX12DescriptorHeap* cbvHeap, uint32_t frameIndex)
{
	//CREATE CMD ALLOC
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_commandAllocator.GetAddressOf())
	));

	//SET HEAP VAR
	//NEED FrameIndex, ThreadIndex, public descNum per frame, private descNum per thread, maxworker // in multi-thread env
	
	//CREATE CONSTANT BUFFER
	//ALLOCATE GPU ADDRESS
	uint32_t elementByteSize[3] =
	{ CalcConstantBufferByteSize(sizeof(PassConstants)),
		CalcConstantBufferByteSize(sizeof(ObjectConstants)),
		CalcConstantBufferByteSize(sizeof(MaterialConstants)) };
	int CBIndex = 0;
	auto descCPUAddress = cbvHeap->Offset(cbvHeap->CalcHeapSliceShareBlock(frameIndex, 0, EngineConfig::SwapChainBufferCount, 0, 0)).cpuDescHandle;

	//PassConstantBuffer
	m_DX12PassConstantBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12PassConstantBuffer->CreateConstantBuffer(device, elementByteSize[CBIndex]);
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_DX12PassConstantBuffer->GetResource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC passCBVDesc;
	passCBVDesc.BufferLocation = cbAddress;
	passCBVDesc.SizeInBytes = elementByteSize[CBIndex];

	m_DX12PassConstantBufferView = std::make_unique<DX12View>(
		device,
		EViewType::EConstantBufferView,
		m_DX12PassConstantBuffer.get(),
		descCPUAddress,
		&passCBVDesc);

	//index offset
	//desc address offset
	descCPUAddress.ptr += cbvHeap->GetDescIncSize();
	CBIndex += 1;
	//Object Constant Buffer
	m_DX12ObjectConstantBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12ObjectConstantBuffer->CreateConstantBuffer(device, elementByteSize[CBIndex]);
	cbAddress = m_DX12ObjectConstantBuffer->GetResource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC objCBVDesc;
	objCBVDesc.BufferLocation = cbAddress;
	objCBVDesc.SizeInBytes = elementByteSize[CBIndex];

	m_DX12ObjectConstantBufferView = std::make_unique<DX12View>(
		device,
		EViewType::EConstantBufferView,
		m_DX12ObjectConstantBuffer.get(),
		descCPUAddress,
		&objCBVDesc);

	//index offset
	//desc address offset
	descCPUAddress.ptr += cbvHeap->GetDescIncSize();
	CBIndex += 1;
	//Material Constant Buffer
	m_DX12MaterialConstantBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12MaterialConstantBuffer->CreateConstantBuffer(device, elementByteSize[CBIndex]);
	cbAddress = m_DX12MaterialConstantBuffer->GetResource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc;
	matCBVDesc.BufferLocation = cbAddress;
	matCBVDesc.SizeInBytes = elementByteSize[CBIndex];

	m_DX12MaterialConstantBufferView = std::make_unique<DX12View>(
		device,
		EViewType::EConstantBufferView,
		m_DX12MaterialConstantBuffer.get(),
		descCPUAddress,
		&matCBVDesc);
}

DX12FrameResource::~DX12FrameResource()
{

}

void DX12FrameResource::EnsureWorkerCapacity(ID3D12Device* device, uint32_t n) {
	if (m_workerAlloc.size() >= n) return;

	size_t old = m_workerAlloc.size();
	m_workerAlloc.resize(n);
	for (size_t i = old; i < n; ++i)
		ThrowIfFailed(device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_workerAlloc[i].GetAddressOf())));
}