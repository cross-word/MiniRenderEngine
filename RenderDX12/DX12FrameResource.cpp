#include "stdafx.h"
#include "DX12FrameResource.h"

DX12FrameResource::DX12FrameResource(ID3D12Device* device)
{
	//CREATE CMD ALLOC
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_commandAllocator.GetAddressOf())
	));
}

DX12FrameResource::~DX12FrameResource()
{

}

void DX12FrameResource::CreateCBVSRV(ID3D12Device* device, ID3D12GraphicsCommandList* m_commandList, DX12DescriptorHeap* cbvHeap, uint32_t frameIndex)
{
	//SET HEAP VAR
//NEED FrameIndex, ThreadIndex, public descNum per frame, private descNum per thread, maxworker // in multi-thread env

//CREATE CONSTANT BUFFER
//ALLOCATE GPU ADDRESS
	uint32_t byteSize = CalcConstantBufferByteSize(sizeof(PassConstants));
	int CBIndex = 0;
	auto descCPUAddress = cbvHeap->Offset(cbvHeap->CalcHeapSliceShareBlock(frameIndex, 0, EngineConfig::ConstantBufferCount, 0, 0)).cpuDescHandle;

	//PassConstantBuffer
	m_DX12PassConstantBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12PassConstantBuffer->CreateConstantBuffer(device, byteSize);
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_DX12PassConstantBuffer->GetResource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC passCBVDesc;
	passCBVDesc.BufferLocation = cbAddress;
	passCBVDesc.SizeInBytes = byteSize;

	m_DX12PassConstantBufferView = std::make_unique<DX12View>(
		device,
		EViewType::EConstantBufferView,
		m_DX12PassConstantBuffer.get(),
		descCPUAddress,
		&passCBVDesc);
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

void DX12FrameResource::UploadObjectConstant(
	ID3D12Device* device, 
	DX12CommandList* DX12CommandList, 
	DX12DescriptorHeap* DX12CBVDDSHeap, 
	std::vector<Render::RenderItem>& renderItems,
	DX12ObjectConstantManager* DX12ObjectConstantManager)
{
	for (size_t i = 0; i < renderItems.size(); ++i)
	{
		if(renderItems[i].IsObjDirty())
		{
			if (DX12ObjectConstantManager->GetObjectConstantCount() < i + 1)
			{
				DX12ObjectConstantManager->PushObjectConstant(renderItems[i].GetObjectConstant());
				renderItems[i].SetObjConstantIndex(i);
			} //new
			else
			{
				DX12ObjectConstantManager->GetObjectConstant(i) = renderItems[i].GetObjectConstant();
			} //update

			DX12ObjectConstantManager->UploadConstant(
				device,
				DX12CommandList,
				sizeof(ObjectConstants),
				DX12ObjectConstantManager->GetObjectConstantData(),
				i * sizeof(ObjectConstants));
			renderItems[i].SetDirtyFlag(false);
		}
	}
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