#include "stdafx.h"
#include "DX12MaterialManager.h"

DX12MaterialManager::DX12MaterialManager()
{

}

DX12MaterialManager::~DX12MaterialManager()
{

}

void DX12MaterialManager::PushMaterialVector(std::unique_ptr<Material>&& Mat)
{
    if (Mat == nullptr) return;
    MaterialConstants tmpMatConst;
    tmpMatConst.DiffuseAlbedo = Mat->matConstant.DiffuseAlbedo;
    tmpMatConst.FresnelR0 = Mat->matConstant.FresnelR0;
    tmpMatConst.MatTransform = Mat->matConstant.MatTransform;
    tmpMatConst.Roughness = Mat->matConstant.Roughness;
	m_materials.emplace_back(std::move(Mat));
    m_materialConstant.emplace_back(tmpMatConst);
}


void DX12MaterialManager::InitialzieUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	if (m_materials.empty())
	{
		::OutputDebugStringA("No Material was Loaded in Material Manager!");
		assert(m_materials.empty());
	}

	m_DX12MaterialResource = std::make_unique<DX12ResourceTexture>();
	m_DX12MaterialResource->CreateMaterialResource(
		device,
		cmdList,
        m_materialConstant.size()
	);
}

void DX12MaterialManager::InitializeSRV(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle)
{
    if (m_materials.empty())
    {
        ::OutputDebugStringA("No Material was Loaded in Material Manager!");
        assert(m_materials.empty());
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_DX12MaterialResource->GetResource()->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = m_materialConstant.size();
    srvDesc.Buffer.StructureByteStride = sizeof(MaterialConstants);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    //view
    m_DX12MaterialView = std::make_unique<DX12View>(
        device,
        EViewType::EShaderResourceView,
        m_DX12MaterialResource.get(),
        *cpuHandle,
        nullptr,
        &srvDesc
    );
}

void DX12MaterialManager::UploadMaterial(ID3D12Device* device, DX12CommandList* dx12CommandList)
{
    UINT byteSize = m_materialConstant.size() * sizeof(MaterialConstants);

    m_DX12MaterialResource->CreateUploadBuffer(device, byteSize);
    m_DX12MaterialResource->CopyAndUploadResource(m_DX12MaterialResource->GetUploadBuffer(), m_materialConstant.data(), byteSize);
    m_DX12MaterialResource->TransitionState(dx12CommandList->GetCommandList(), D3D12_RESOURCE_STATE_COPY_DEST);
    dx12CommandList->GetCommandList()->CopyBufferRegion(m_DX12MaterialResource->GetResource(), 0, m_DX12MaterialResource->GetUploadBuffer(), 0, byteSize);
    m_DX12MaterialResource->TransitionState(dx12CommandList->GetCommandList(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    dx12CommandList->SubmitAndWait();
    m_DX12MaterialResource->ResetUploadBuffer();
}