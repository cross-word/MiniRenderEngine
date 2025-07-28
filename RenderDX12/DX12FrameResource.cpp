#include "stdafx.h"
#include "DX12FrameResource.h"

DX12FrameResource::DX12FrameResource(ID3D12Device* device, DX12DescriptorHeap* cbvHeap)
{
	//CREATE CMD ALLOC
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_commandAllocator.GetAddressOf())
	));

	//CREATE BUFFER
	//ALLOCATE GPU ADDRESS
	UINT elementByteSize[3] =
	{ CalcConstantBufferByteSize(sizeof(PassConstants)),
		CalcConstantBufferByteSize(sizeof(ObjectConstants)),
		CalcConstantBufferByteSize(sizeof(MaterialConstants)) };
	int CBIndex = 0;
	auto descCPUAddress = cbvHeap->GetDescHeap()->GetCPUDescriptorHandleForHeapStart();

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