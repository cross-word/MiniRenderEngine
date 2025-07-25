#include "stdafx.h"
#include "DX12Resource.h"

DX12Resource::DX12Resource()
{

}

DX12Resource::DX12Resource(D3D12_RESOURCE_STATES  newInitialState)
{
	m_currentState = newInitialState;
}

DX12Resource::~DX12Resource()
{

}

void DX12Resource::TransitionState(ID3D12GraphicsCommandList* m_commandList, D3D12_RESOURCE_STATES newState)
{
	assert(m_resource);
	if (m_currentState == newState) return;

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_resource.Get(), m_currentState, newState);
	m_commandList->ResourceBarrier(1, &barrier);
	m_currentState = newState;

	return;
}

void DX12ResourceBuffer::CreateConstantBuffer(ID3D12Device* m_device)
{
	UINT elementByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(elementByteSize);
	ThrowIfFailed(m_device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_resource)
	));
	m_currentState = D3D12_RESOURCE_STATE_GENERIC_READ;
}

void DX12ResourceBuffer::CreateVertexBuffer(ID3D12Device* m_device, Vertex* vertex, UINT vertexBufferSize)
{
	// Define the geometry for a triangle
/*
Vertex triangleVertices[] =
{
	{ { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
	{ { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
	{ { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
};
*/

	// Note: using upload heaps to transfer static data like vert buffers is not 
	// recommended. Every time the GPU needs it, the upload heap will be marshalled 
	// over. Please read up on Default Heap usage. An upload heap is used here for 
	// code simplicity and because there are very few verts to actually transfer.

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
	ThrowIfFailed(m_device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_resource)));
	m_currentState = D3D12_RESOURCE_STATE_GENERIC_READ;
}

void DX12ResourceBuffer::UploadConstantBuffer(void* sourceAddress, size_t dataSize)
{
	UINT8* pData;
	ThrowIfFailed(m_resource->Map(0, nullptr, reinterpret_cast<void**>(&pData)));
	memcpy(pData, sourceAddress, dataSize);
	m_resource->Unmap(0, nullptr);
}

void DX12ResourceBuffer::UploadConstantBuffer(CD3DX12_RANGE readRange, void* sourceAddress, size_t dataSize)
{
	UINT8* pData;
	ThrowIfFailed(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&pData)));
	memcpy(pData, sourceAddress, dataSize);
	m_resource->Unmap(0, nullptr);
}

void DX12ResourceTexture::CreateDepthStencil(
	ID3D12Device* m_device,
	UINT clientWidth,
	UINT clientHeight,
	UINT sampleDescCount,
	UINT sampleDescQuality,
	DXGI_FORMAT m_depthStencilFormat)
{
	//create DSV
	D3D12_RESOURCE_DESC DSVDesc = {};
	DSVDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DSVDesc.Alignment = 0;
	DSVDesc.Width = clientWidth;
	DSVDesc.Height = clientHeight;
	DSVDesc.DepthOrArraySize = 1;
	DSVDesc.MipLevels = 1;
	DSVDesc.Format = m_depthStencilFormat;
	DSVDesc.SampleDesc.Count = sampleDescCount;
	DSVDesc.SampleDesc.Quality = sampleDescQuality;
	DSVDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	DSVDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = m_depthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(m_device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&DSVDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(&m_resource))
	);
	m_currentState = D3D12_RESOURCE_STATE_COMMON;
}