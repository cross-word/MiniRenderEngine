#pragma once

#include <span>

#include "stdafx.h"
#include "DX12CommandList.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12RESOURCE
MAIN WORK:
1. interface of DX12ResourceBuffer, DX12ResourceTexture
2. make resource
3. manage Resource State
MAIN MEMBER:
1. m_resource
2. m_currentState
3. TransitionState() ****** Every Resource State must be change through this class
*/
class DX12Resource
{
public:
	DX12Resource();
	DX12Resource(D3D12_RESOURCE_STATES  newInitialState);
	~DX12Resource();

	ID3D12Resource* GetResource() const noexcept { return m_resource.Get(); }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVAdress() const noexcept { return m_resource->GetGPUVirtualAddress(); }
	D3D12_RESOURCE_STATES GetCurrentState() const noexcept { return m_currentState; }

	void TransitionState(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES newState); //실제 상태전이는 커맨드 실행시에 전이됨, 수정해야함

	void Reset() noexcept { m_resource.Reset(); m_currentState = D3D12_RESOURCE_STATE_COMMON; }
	ID3D12Resource** GetAddressOf() noexcept { return m_resource.GetAddressOf(); }
protected:
	ComPtr<ID3D12Resource> m_resource = nullptr;
	D3D12_RESOURCE_STATES  m_currentState = D3D12_RESOURCE_STATE_COMMON; //default state common
};

class DX12ResourceBuffer : public DX12Resource
{
private:
	ComPtr<ID3D12Resource> m_uploadBuffer;
public:
	void CreateConstantBuffer(ID3D12Device* device);
    void CreateVertexBuffer(ID3D12Device* device, std::span<const Vertex> vertices, ID3D12GraphicsCommandList* cmdList);
	void CreateIndexBuffer(ID3D12Device* device, std::span<const uint32_t> indices, ID3D12GraphicsCommandList* cmdList);
	void CopyAndUploadResource(ID3D12Resource* uploadBuffer, const void* sourceAddress, size_t dataSize, CD3DX12_RANGE* readRange = nullptr);

};

class DX12ResourceTexture : public DX12Resource
{
private:

public:
	void CreateDepthStencil(
		ID3D12Device* device,
		UINT clientWidth,
		UINT clientHeight,
		UINT multiSampleDescCount,
		DXGI_FORMAT depthStencilFormat);

	void CreateRenderTarget(
		ID3D12Device* device,
		UINT clientWidth,
		UINT clientHeight,
		UINT multiSampleDescCount,
		DXGI_FORMAT renderTargetFormat);
};