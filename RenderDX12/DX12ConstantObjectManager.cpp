#include "stdafx.h"
#include "DX12ConstantObjectManager.h"

DX12ConstantObjectManager::DX12ConstantObjectManager()
{

}

DX12ConstantObjectManager::~DX12ConstantObjectManager()
{

}

void DX12ConstantObjectManager::InitialzieUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT byteSize)
{
    m_DX12ConstantUploader = std::make_unique<DX12ResourceTexture>();
    m_DX12ConstantUploader->CreateMaterialorObjectResource(
		device,
		cmdList,
        byteSize
	);
}

void DX12ConstantObjectManager::InitializeSRV(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, UINT numConstants)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_DX12ConstantUploader->GetResource()->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = numConstants;
    srvDesc.Buffer.StructureByteStride = sizeof(MaterialConstants);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    //view
    m_DX12UploaderView = std::make_unique<DX12View>(
        device,
        EViewType::EShaderResourceView,
        m_DX12ConstantUploader.get(),
        *cpuHandle,
        nullptr,
        &srvDesc
    );
}

void DX12ConstantObjectManager::UploadConstant(ID3D12Device* device, DX12CommandList* dx12CommandList, UINT byteSize, const void* sourceAddress)
{
    m_DX12ConstantUploader->CreateUploadBuffer(device, byteSize);
    m_DX12ConstantUploader->CopyAndUploadResource(m_DX12ConstantUploader->GetUploadBuffer(), sourceAddress, byteSize);
    m_DX12ConstantUploader->TransitionState(dx12CommandList->GetCommandList(), D3D12_RESOURCE_STATE_COPY_DEST);
    dx12CommandList->GetCommandList()->CopyBufferRegion(m_DX12ConstantUploader->GetResource(), 0, m_DX12ConstantUploader->GetUploadBuffer(), 0, byteSize);
    m_DX12ConstantUploader->TransitionState(dx12CommandList->GetCommandList(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void DX12MaterialConstantManager::InitialzieUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT numConstants)
{
    if (m_materials.empty())
    {
        ::OutputDebugStringA("No Material was Loaded in Material Manager!");
        assert(m_materials.empty());
    }
    DX12ConstantObjectManager::InitialzieUploadBuffer(device, cmdList, numConstants);
}

void DX12MaterialConstantManager::InitializeSRV(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, UINT numConstants)
{
    if (m_materials.empty())
    {
        ::OutputDebugStringA("No Material was Loaded in Material Manager!");
        assert(m_materials.empty());
    }
    DX12ConstantObjectManager::InitializeSRV(device, cpuHandle, numConstants);
}

void DX12MaterialConstantManager::PushMaterial(std::unique_ptr<Material>&& Mat)
{
    if (Mat == nullptr) return;

    MaterialConstants tmpMatConst;
    tmpMatConst.DiffuseAlbedo = Mat->matConstant.DiffuseAlbedo;
    tmpMatConst.FresnelR0 = Mat->matConstant.FresnelR0;
    tmpMatConst.MatTransform = Mat->matConstant.MatTransform;
    tmpMatConst.Roughness = Mat->matConstant.Roughness;

    m_materialConstant.emplace_back(tmpMatConst);
    m_materials.emplace_back(std::move(Mat));
}

void DX12ObjectConstantManager::InitialzieUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT numConstants)
{
    if (m_objectConstants.empty())
    {
        ::OutputDebugStringA("No ObjectConstant was Loaded in ObjectConstant Manager!");
        assert(m_objectConstants.empty());
    }
    DX12ConstantObjectManager::InitialzieUploadBuffer(device, cmdList, numConstants);
}

void DX12ObjectConstantManager::InitializeSRV(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, UINT numConstants)
{
    if (m_objectConstants.empty())
    {
        ::OutputDebugStringA("No ObjectConstant was Loaded in ObjectConstant Manager!");
        assert(m_objectConstants.empty());
    }
    DX12ConstantObjectManager::InitializeSRV(device, cpuHandle, numConstants);
}

void DX12ObjectConstantManager::PushObjectConstant(ObjectConstants Obj)
{
    m_objectConstants.emplace_back(Obj);
}