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

	bool InitMeshFromFile(
		ID3D12Device* device,
		DX12FrameResource* DX12FrameResource,
		DX12CommandList* dx12CommandList,
		const std::wstring& filename,
		UINT prevIndexCount = 0,
		UINT prevVertexCount = 0,
		D3D12_PRIMITIVE_TOPOLOGY vertexPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); //.obj file
	inline DX12View* GetDX12VertexBufferView() const { return m_DX12VertexView.get(); }
	inline DX12View* GetDX12IndexBufferView() const { return m_DX12IndexView.get(); }

	inline D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveTopologyType() const { return m_primitiveTopologyType; }
	inline UINT GetIndexCount() const noexcept { return m_IndexCount; }
	inline UINT GetVertexCount() const noexcept { return m_VertexCount; }
	inline UINT GetStartIndexLocation() const noexcept { return m_StartIndexLocation; }
	inline UINT GetBaseVertexLocation() const noexcept { return m_BaseVertexLocation; }

private:
	//vertex
	//index
	std::unique_ptr<DX12ResourceBuffer> m_DX12VertexBuffer;
	std::unique_ptr<DX12View> m_DX12VertexView;

	std::unique_ptr<DX12ResourceBuffer> m_DX12IndexBuffer;
	std::unique_ptr<DX12View> m_DX12IndexView;
	DXGI_FORMAT m_indexFormat = DXGI_FORMAT_R32_UINT;

	//rendering info
	D3D12_PRIMITIVE_TOPOLOGY m_primitiveTopologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; //default
	UINT m_IndexCount = 0;
	UINT m_VertexCount = 0;
	UINT m_StartIndexLocation = 0;
	UINT m_BaseVertexLocation = 0;
};