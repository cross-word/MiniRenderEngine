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
	uint32_t elementByteSize[2] =
	{ CalcConstantBufferByteSize(sizeof(PassConstants)),
		CalcConstantBufferByteSize(sizeof(ObjectConstants))};
	int CBIndex = 0;
	auto descCPUAddress = cbvHeap->Offset(cbvHeap->CalcHeapSliceShareBlock(frameIndex, 0, EngineConfig::ConstantBufferCount, 0, 0)).cpuDescHandle;

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
	/*
	D3D12_CONSTANT_BUFFER_VIEW_DESC objCBVDesc;
	objCBVDesc.BufferLocation = cbAddress;
	objCBVDesc.SizeInBytes = elementByteSize[CBIndex];

	m_DX12ObjectConstantBufferView = std::make_unique<DX12View>(
		device,
		EViewType::EConstantBufferView,
		m_DX12ObjectConstantBuffer.get(),
		descCPUAddress,
		&objCBVDesc);
		*/
}

DX12FrameResource::~DX12FrameResource()
{

}

void DX12FrameResource::UploadPassConstant(D3DCamera* d3dCamera)
{
	PassConstants passConst;
	passConst.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	passConst.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	passConst.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	passConst.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	passConst.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	passConst.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	passConst.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };
	using namespace DirectX;

	XMMATRIX V = d3dCamera->GetViewMatrix();
	XMMATRIX P = d3dCamera->GetProjectionMatrix(float(EngineConfig::DefaultWidth) / float(EngineConfig::DefaultHeight));
	XMMATRIX VP = XMMatrixMultiply(V, P); // HLSL column-major
	XMMATRIX iV = XMMatrixInverse(nullptr, V);
	XMMATRIX iP = XMMatrixInverse(nullptr, P);
	XMMATRIX iVP = XMMatrixInverse(nullptr, VP);

	XMStoreFloat4x4(&passConst.View, XMMatrixTranspose(V));
	XMStoreFloat4x4(&passConst.Proj, XMMatrixTranspose(P));
	XMStoreFloat4x4(&passConst.ViewProj, XMMatrixTranspose(VP));
	XMStoreFloat4x4(&passConst.InvView, XMMatrixTranspose(iV));
	XMStoreFloat4x4(&passConst.InvProj, XMMatrixTranspose(iP));
	XMStoreFloat4x4(&passConst.InvViewProj, XMMatrixTranspose(iVP));

	m_DX12PassConstantBuffer->CopyAndUploadResource(
		m_DX12PassConstantBuffer->GetResource(),
		&passConst,
		sizeof(PassConstants));
}

void DX12FrameResource::UploadObjectConstant(D3DCamera* d3dCamera)
{
	ObjectConstants objConst;
	//objConst.World = XMMatrixTranspose(XMMatrixIdentity() * d3dCamera->GetViewMatrix() * d3dCamera->GetProjectionMatrix(float(EngineConfig::DefaultWidth / EngineConfig::DefaultHeight))); //TMP CODE FOR TEST CBV TABLE
	m_DX12ObjectConstantBuffer->CopyAndUploadResource(
		m_DX12ObjectConstantBuffer->GetResource(),
		&objConst,
		sizeof(ObjectConstants));
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