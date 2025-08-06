#pragma once

#include "stdafx.h"

#include "DX12Resource.h"
#include "DX12View.h"

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

namespace Render
{
	struct RenderItem
	{
	public:
		void SetRenderGeometry(DX12RenderGeometry* renderGeo) { m_renderGeomery = renderGeo; }
		void SetTextureIndex(UINT index) { m_textureIndex = index; }
		void SetMaterialIndex(UINT index) { m_materialIndex = index; }
		void SetStartIndexLocation(UINT index) { m_startIndexLocation = index; }
		void SetBaseVertexLocation(UINT index) { m_baseVertexLocation = index; }
		void SetObjWorldMatrix(const XMFLOAT4X4& mat) { m_objConst.World = mat; m_bObjDirty = true; }
		void SetObjTransformMatrix(const XMFLOAT4X4& mat) { m_objConst.TexTransform = mat; m_bObjDirty = true; }
		void SetObjConstantIndex(UINT index) { m_objectIndex = index; }
		void SetDirtyFlag(bool b) { m_bObjDirty = b; }

		DX12RenderGeometry* GetRenderGeometry() { return m_renderGeomery; }
		UINT GetTextureIndex() { return m_textureIndex; }
		UINT GetMaterialIndex() { return m_materialIndex; }
		UINT GetStartIndexLocation() { return m_startIndexLocation; }
		UINT GetBaseVertexLocation() { return m_baseVertexLocation; }
		ObjectConstants& GetObjectConstant() { return m_objConst; }
		bool IsObjDirty() { return m_bObjDirty; }
		UINT GetObjectConstantIndex() { return m_objectIndex; }
	private:
		DX12RenderGeometry* m_renderGeomery;
		ObjectConstants m_objConst;

		UINT m_textureIndex = 0;
		UINT m_materialIndex = 0;
		UINT m_startIndexLocation = 0; // + previous index size
		UINT m_baseVertexLocation = 0; // + previous vertex size
		UINT m_objectIndex = 0;
		bool m_bObjDirty = true;
	};
}