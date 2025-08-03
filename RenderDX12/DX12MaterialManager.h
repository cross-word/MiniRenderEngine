#pragma once

#include "stdafx.h"

#include "D3DUtil.h"
#include "DX12Resource.h"
#include "DX12View.h"

class DX12MaterialManager
{
public:
	DX12MaterialManager();
	~DX12MaterialManager();

	void PushMaterialVector(std::unique_ptr<Material>&& Mat);
	void InitialzieUploadBuffer(ID3D12Device* device,ID3D12GraphicsCommandList* cmdList);
	void UploadMaterial(ID3D12Device* device, DX12CommandList* dx12CommandList);
	void InitializeSRV(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle);

	inline DX12ResourceTexture* GetMaterialResource() const noexcept { return m_DX12MaterialResource.get(); }
	inline DX12View* GetDX12MaterialView() const noexcept { return m_DX12MaterialView.get(); }
	inline uint32_t GetMaterialCount() const noexcept { return m_materials.size(); }
	inline Material* GetMaterial(UINT index) noexcept { return m_materials[index].get(); }
	inline bool IsMaterailEmpty() const noexcept { return m_materials.empty(); }
private:
	std::vector<std::unique_ptr<Material>> m_materials;
	std::vector<MaterialConstants> m_materialConstant;
    std::unique_ptr<DX12ResourceTexture> m_DX12MaterialResource;
    std::unique_ptr<DX12View> m_DX12MaterialView;
};