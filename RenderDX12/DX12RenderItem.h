#pragma once

#include "stdafx.h"

#include "DX12Resource.h"
#include "DX12View.h"
#include "DX12FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class DX12RenderItem
{
public:
	DX12RenderItem();
	~DX12RenderItem();

	void InitMeshFromFile(
		ID3D12Device* device,
		DX12FrameResource* DX12FrameResource,
		DX12CommandList* dx12CommandList,
		const std::wstring& filename,
		uint32_t currentFenceValue); //.obj file
	inline DX12View* GetDX12VertexBufferView() const { return m_DX12VertexView.get(); }
	inline DX12View* GetDX12IndexBufferView() const { return m_DX12IndexView.get(); }
private:
	//vertex
	//index
	//mat
	std::unique_ptr<DX12ResourceBuffer> m_DX12VertexBuffer;
	std::unique_ptr<DX12View> m_DX12VertexView;

	std::unique_ptr<DX12ResourceBuffer> m_DX12IndexBuffer;
	std::unique_ptr<DX12View> m_DX12IndexView;
	DXGI_FORMAT m_indexFormat = DXGI_FORMAT_R32_UINT;
};