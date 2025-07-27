#include "stdafx.h"
#include "DX12Resource.h"
#include "../MiniEngineCore/EngineConfig.h"

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

void DX12Resource::TransitionState(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES newState)
{
	assert(m_resource);
	if (m_currentState == newState) return;

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_resource.Get(), m_currentState, newState);
	commandList->ResourceBarrier(1, &barrier);
	m_currentState = newState;

	return;
}

void DX12ResourceBuffer::CreateConstantBuffer(ID3D12Device* device)
{
	UINT elementByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants::WorldViewProj));

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(elementByteSize);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_resource)
	));
	m_currentState = D3D12_RESOURCE_STATE_GENERIC_READ;
}

void DX12ResourceBuffer::CreateVertexBuffer(ID3D12Device* device, std::span<const Vertex> vertices, ID3D12GraphicsCommandList* cmdList)
{
	// resourcse which do not change during frame upload to GPU in initial time!!

	const UINT64 vertexSize = static_cast<UINT64>(vertices.size()) * sizeof(Vertex);
	if (vertexSize == 0) { m_resource.Reset(); m_uploadBuffer.Reset(); return; }

	CD3DX12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC descBuffer = CD3DX12_RESOURCE_DESC::Buffer(vertexSize);
	// vertex buffer
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&descBuffer,
		D3D12_RESOURCE_STATE_COMMON, //DEFAULT 버퍼의 초기 상태는 무조건 COMMON (작성해도 무시됨)
		nullptr,
		IID_PPV_ARGS(&m_resource)));
	m_currentState = D3D12_RESOURCE_STATE_COMMON;
	TransitionState(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	CD3DX12_HEAP_PROPERTIES heapProp2 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC descBuffer2 = CD3DX12_RESOURCE_DESC::Buffer(vertexSize);
	// upload buffer
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProp2,
		D3D12_HEAP_FLAG_NONE,
		&descBuffer2,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_uploadBuffer.ReleaseAndGetAddressOf())));
	m_uploadBufferCurrentState = D3D12_RESOURCE_STATE_GENERIC_READ;

	CopyAndUploadResource(m_uploadBuffer.Get(), vertices.data(), static_cast<size_t>(vertexSize));
	cmdList->CopyBufferRegion(m_resource.Get(), 0, m_uploadBuffer.Get(), 0, vertexSize); //명령 실행까지 uploadbuffer 살아있어야함 => 어떻게 보장하게?
	TransitionState(cmdList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}

void DX12ResourceBuffer::CreateIndexBuffer(ID3D12Device* device, std::span<const uint32_t> indices, ID3D12GraphicsCommandList* cmdList)
{
	// resourcse which do not change during frame upload to GPU in initial time!!

	const UINT64 indexSize = static_cast<UINT64>(indices.size()) * sizeof(uint32_t);
	if (indexSize == 0) { m_resource.Reset(); m_uploadBuffer.Reset(); return; }

	CD3DX12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC descBuffer = CD3DX12_RESOURCE_DESC::Buffer(indexSize);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&descBuffer,
		D3D12_RESOURCE_STATE_COMMON, //DEFAULT 버퍼의 초기 상태는 무조건 COMMON (작성해도 무시됨)
		nullptr,
		IID_PPV_ARGS(&m_resource)));
	m_currentState = D3D12_RESOURCE_STATE_COMMON;
	TransitionState(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	CD3DX12_HEAP_PROPERTIES heapProp2 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC descBuffer2 = CD3DX12_RESOURCE_DESC::Buffer(indexSize);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProp2,
		D3D12_HEAP_FLAG_NONE,
		&descBuffer2,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_uploadBuffer.ReleaseAndGetAddressOf())));
	m_uploadBufferCurrentState = D3D12_RESOURCE_STATE_GENERIC_READ;

	CopyAndUploadResource(m_uploadBuffer.Get(), indices.data(), static_cast<size_t>(indexSize));
	cmdList->CopyBufferRegion(m_resource.Get(), 0, m_uploadBuffer.Get(), 0, indexSize); //명령 실행까지 uploadbuffer 살아있어야함 => 어떻게 보장하게?
	TransitionState(cmdList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
}

void DX12ResourceBuffer::CopyAndUploadResource(ID3D12Resource* uploadBuffer, const void* sourceAddress, size_t dataSize, CD3DX12_RANGE* readRange)
{
	void* mapped = nullptr;
	ThrowIfFailed(uploadBuffer->Map(0, readRange, &mapped));
	memcpy(mapped, sourceAddress, dataSize);
	uploadBuffer->Unmap(0, nullptr);
}

void DX12ResourceTexture::CreateDepthStencil(
	ID3D12Device* device,
	UINT clientWidth,
	UINT clientHeight,
	UINT multiSampleDescCount,
	DXGI_FORMAT depthStencilFormat)
{
	//create DSDesc
	D3D12_RESOURCE_DESC DSVDesc = {};
	DSVDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DSVDesc.Alignment = 0;
	DSVDesc.Width = clientWidth;
	DSVDesc.Height = clientHeight;
	DSVDesc.DepthOrArraySize = 1;
	DSVDesc.MipLevels = 1;
	DSVDesc.Format = depthStencilFormat;
	DSVDesc.SampleDesc.Count = multiSampleDescCount;
	DSVDesc.SampleDesc.Quality = 0;
	DSVDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	DSVDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = depthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&DSVDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(&m_resource))
	);
	m_currentState = D3D12_RESOURCE_STATE_COMMON;
}

void DX12ResourceTexture::CreateRenderTarget(
	ID3D12Device* device,
	UINT clientWidth,
	UINT clientHeight,
	UINT multiSampleDescCount,
	DXGI_FORMAT renderTargetFormat)
{
	//create RTVDesc
	D3D12_RESOURCE_DESC RTVDesc = {};
	RTVDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	RTVDesc.Alignment = 0;
	RTVDesc.Width = clientWidth;
	RTVDesc.Height = clientHeight;
	RTVDesc.DepthOrArraySize = 1;
	RTVDesc.MipLevels = 1;
	RTVDesc.Format = renderTargetFormat;
	RTVDesc.SampleDesc.Count = multiSampleDescCount;
	RTVDesc.SampleDesc.Quality = 0;
	RTVDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	RTVDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Color[0] = EngineConfig::DefaultClearColor[0];
	optClear.Color[1] = EngineConfig::DefaultClearColor[1];
	optClear.Color[2] = EngineConfig::DefaultClearColor[2];
	optClear.Color[3] = EngineConfig::DefaultClearColor[3];
	optClear.Format = renderTargetFormat;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&RTVDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&optClear,
		IID_PPV_ARGS(&m_resource))
	);
	m_currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
}