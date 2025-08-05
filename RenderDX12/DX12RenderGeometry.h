#pragma once

#include "stdafx.h"

#include "DX12Resource.h"
#include "DX12View.h"
#include "DX12FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class DX12RenderGeometry
{
public:
	DX12RenderGeometry();
	~DX12RenderGeometry();

	DX12RenderGeometry operator=(DX12RenderGeometry& moveGeo)
	{
		m_DX12VertexBuffer = moveGeo.m_DX12VertexBuffer;
		m_DX12VertexView = moveGeo.m_DX12VertexView;
		m_DX12IndexBuffer = moveGeo.m_DX12IndexBuffer;
		m_DX12IndexView = moveGeo.m_DX12IndexView;
		m_indexFormat = moveGeo.m_indexFormat;
		m_primitiveTopologyType = moveGeo.m_primitiveTopologyType;
		m_IndexCount = moveGeo.m_IndexCount;
		m_VertexCount = moveGeo.m_VertexCount;
	}

	bool InitMeshFromFile(
		ID3D12Device* device,
		DX12FrameResource* DX12FrameResource,
		DX12CommandList* dx12CommandList,
		const std::wstring& filename,
		D3D12_PRIMITIVE_TOPOLOGY vertexPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); //.obj file
	inline DX12ResourceBuffer* GetDX12VertexBuffer() const noexcept { return m_DX12VertexBuffer.get(); }
	inline DX12ResourceBuffer* GetDX12IndexBuffer() const noexcept { return m_DX12IndexBuffer.get(); }
	inline DX12View* GetDX12VertexBufferView() const noexcept { return m_DX12VertexView.get(); }
	inline DX12View* GetDX12IndexBufferView() const noexcept { return m_DX12IndexView.get(); }

	inline D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveTopologyType() const { return m_primitiveTopologyType; }
	inline UINT GetIndexCount() const noexcept { return m_IndexCount; }
	inline UINT GetVertexCount() const noexcept { return m_VertexCount; }

private:
	//vertex
	//index
	std::shared_ptr<DX12ResourceBuffer> m_DX12VertexBuffer;
	std::shared_ptr<DX12View> m_DX12VertexView;

	std::shared_ptr<DX12ResourceBuffer> m_DX12IndexBuffer;
	std::shared_ptr<DX12View> m_DX12IndexView;
	DXGI_FORMAT m_indexFormat = DXGI_FORMAT_R32_UINT;

	//rendering info
	D3D12_PRIMITIVE_TOPOLOGY m_primitiveTopologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; //default
	UINT m_IndexCount = 0;
	UINT m_VertexCount = 0;
};