#pragma once

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

	void TransitionState(ID3D12GraphicsCommandList* m_commandList, D3D12_RESOURCE_STATES newState);

	void Reset() noexcept { m_resource.Reset(); m_currentState = D3D12_RESOURCE_STATE_COMMON; }
	ID3D12Resource** GetAddressOf() noexcept { return m_resource.GetAddressOf(); }
protected:
	ComPtr<ID3D12Resource> m_resource = nullptr;
	D3D12_RESOURCE_STATES  m_currentState = D3D12_RESOURCE_STATE_COMMON; //default state common
};

class DX12ResourceBuffer : public DX12Resource
{
private:

public:
	void CreateConstantBuffer(ID3D12Device* m_device);
    void CreateVertexBuffer(ID3D12Device* m_device, Vertex* vertex, UINT vertexBufferSize);
    void UploadConstantBuffer(void* sourceAddress, size_t DataSize);
    void UploadConstantBuffer(CD3DX12_RANGE readRange, void* sourceAddress, size_t dataSize);

};

class DX12ResourceTexture : public DX12Resource
{
private:

public:
	void CreateDepthStencil(
		ID3D12Device* m_device,
		UINT clientWidth,
		UINT clientHeight,
		UINT sampleDescCount,
		UINT sampleDescQuality,
		DXGI_FORMAT m_depthStencilFormat);
};