#pragma once

#include "stdafx.h"
#include "DX12CommandList.h"
#include "DX12RootSignature.h"
#include "DX12PSO.h"
#include "DX12Resource.h"
#include "DX12View.h"
#include "DX12DescriptorHeap.h"
#include "DX12SwapChain.h"

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
	inline DX12DescriptorHeap* GetDX12RTVHeap() const noexcept { return m_DX12rtvHeap.get(); }
	inline DX12DescriptorHeap* GetDX12CBVHeap() const noexcept { return m_DX12cbvHeap.get(); }
	inline DX12DescriptorHeap* GetDX12DSVHeap() const noexcept { return m_DX12dsvHeap.get(); }
	inline DX12CommandList* GetDX12CommandList() const noexcept { return m_DX12CommandList.get(); }
	inline DX12RootSignature* GetDX12RootSignature() const noexcept { return m_DX12RootSignature.get(); }
	inline DX12PSO* GetDX12PSO() const noexcept { return m_DX12PSO.get(); }
	inline DX12View* GetDX12VertexBufferView() const { return m_DX12vertexView.get(); }
	inline DX12SwapChain* GetDX12SwapChain() const noexcept { return m_DX12swapChain.get(); }

	void CreateDX12PSO();
	void PrepareInitialResource();
	void UpdateConstantBuffer();
private:
	void InitDX12CommandList();
	void InitDX12RootSignature();
	void InitShader(); //temp func
	void InitConstantBuffer();
	void InitVertex();
private:
	ComPtr<IDXGIFactory4> m_factory;
	ComPtr<ID3D12Device> m_device;
	std::unique_ptr<DX12CommandList> m_DX12CommandList;
	std::unique_ptr<DX12RootSignature> m_DX12RootSignature;
	std::unique_ptr<DX12PSO> m_DX12PSO;

	//initial resources
	std::unique_ptr<DX12ResourceBuffer> m_DX12constantBuffer;
	std::unique_ptr<DX12ResourceBuffer> m_DX12vertexBuffer;
	std::unique_ptr<DX12SwapChain> m_DX12swapChain;

	std::unique_ptr<DX12DescriptorHeap> m_DX12rtvHeap;
	std::unique_ptr<DX12DescriptorHeap> m_DX12cbvHeap;
	std::unique_ptr<DX12DescriptorHeap> m_DX12dsvHeap;

	std::unique_ptr<DX12View> m_DX12constantBufferView;
	std::unique_ptr<DX12View> m_DX12vertexView;

	///tmp variable
	ComPtr<ID3DBlob> m_vertexShader = nullptr;
	ComPtr<ID3DBlob> m_pixelShader = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

};