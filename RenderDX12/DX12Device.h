#pragma once

#include "stdafx.h"
#include "DX12CommandList.h"
#include "DX12RootSignature.h"
#include "DX12PSO.h"
#include "DX12Resource.h"
#include "DX12View.h"
#include "DX12DescriptorHeap.h"
#include "DX12SwapChain.h"
#include "DX12FrameResource.h"
#include "DX12RenderItem.h"

#include "D3DCamera.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12Device
MAIN WORK:
1. initialize D3D12device
2. initialize and manage DX12Resource/DescriptorHeaps/PSO/Views/SwapChain/CommandList
3. communicate with RenderDX12 and DX12FrameBuffer directly to make them access Resources safely
IMPORTANT MEMBER:
1. m_DX12 members
*/

class DX12Device
{
public:
	DX12Device();
	~DX12Device();
	void Initialize(HWND hWnd);

	inline ID3D12Device* GetDevice() const noexcept { return m_device.Get(); }
	inline DX12DescriptorHeap* GetDX12RTVHeap() const noexcept { return m_DX12RTVHeap.get(); }
	inline DX12DescriptorHeap* GetDX12CBVHeap() const noexcept { return m_DX12CBVHeap.get(); }
	inline DX12DescriptorHeap* GetDX12SRVHeap() const noexcept { return m_DX12SRVHeap.get(); }
	inline DX12DescriptorHeap* GetDX12DSVHeap() const noexcept { return m_DX12DSVHeap.get(); }
	inline DX12CommandList* GetDX12CommandList() const noexcept { return m_DX12CommandList.get(); }
	inline DX12RootSignature* GetDX12RootSignature() const noexcept { return m_DX12RootSignature.get(); }
	inline DX12PSO* GetDX12PSO() const noexcept { return m_DX12PSO.get(); }
	inline DX12RenderItem* GetDX12RenderItem(uint32_t index) const noexcept { return m_DX12RenderItem[index].get() ; }
	inline size_t GetRenderItemSize() const noexcept { return m_DX12RenderItem.size(); }
	inline DX12SwapChain* GetDX12SwapChain() const noexcept { return m_DX12SwapChain.get(); }
	inline HANDLE GetFenceEvent() const noexcept { return m_fenceEvent; }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetOffsetCPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE start, UINT index, UINT handleIncrementSize)
	{
		start.ptr += SIZE_T(index) * handleIncrementSize;
		return start;
	}
	inline D3DCamera* GetD3DCamera() const noexcept { return m_camera.get(); }
	inline UINT GetCurrentBackBufferIndex() const noexcept { return m_currBackBufferIndex; }
	inline DX12FrameResource* GetFrameResource(UINT currBackBufferIndex) const noexcept { return m_DX12FrameResource[currBackBufferIndex].get(); }

	inline void SetCurrentBackBufferIndex(UINT newIndex) { m_currBackBufferIndex = newIndex; }
	void CreateDX12PSO();
	void PrepareInitialResource();
	void UpdateFrameResource();
private:
	void InitDX12CommandList(ID3D12CommandAllocator* commandAllocator);
	void InitDX12SwapChain(HWND hWnd);
	void InitDX12RTVDescHeap();
	void InitDX12DSVDescHeap();
	void InitDX12CBVHeap();
	void InitDX12SRVHeap();
	void InitDX12RootSignature();
	void InitShader(); //temp func

	void InitDX12FrameResource();
private:
	ComPtr<IDXGIFactory4> m_factory;
	ComPtr<ID3D12Device> m_device;
	std::unique_ptr<DX12CommandList> m_DX12CommandList;
	std::unique_ptr<DX12RootSignature> m_DX12RootSignature;
	std::unique_ptr<DX12PSO> m_DX12PSO;

	//initial resources
	uint32_t m_currBackBufferIndex = 0;
	DX12FrameResource* m_DX12CurrFrameResource;
	std::vector<std::unique_ptr<DX12FrameResource>> m_DX12FrameResource;
	std::vector<std::unique_ptr<DX12RenderItem>> m_DX12RenderItem;

	std::unique_ptr<DX12SwapChain> m_DX12SwapChain;

	std::unique_ptr<DX12DescriptorHeap> m_DX12RTVHeap;
	std::unique_ptr<DX12DescriptorHeap> m_DX12CBVHeap;
	std::unique_ptr<DX12DescriptorHeap> m_DX12SRVHeap;
	std::unique_ptr<DX12DescriptorHeap> m_DX12DSVHeap;

	///tmp variable
	ComPtr<ID3DBlob> m_vertexShader = nullptr;
	ComPtr<ID3DBlob> m_pixelShader = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

	std::unique_ptr <D3DCamera> m_camera = std::make_unique<D3DCamera>();
	HANDLE m_fenceEvent = nullptr;
};