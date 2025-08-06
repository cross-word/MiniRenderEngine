#include "stdafx.h"
#include "DX12ConstantManager.h"

DX12ConstantManager::DX12ConstantManager()
{

}

DX12ConstantManager::~DX12ConstantManager()
{

}

void DX12ConstantManager::InitialzieUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT byteSize)
{
    m_DX12ConstantUploader = std::make_unique<DX12ResourceTexture>();
    m_DX12ConstantUploader->CreateMaterialorObjectResource(
		device,
		cmdList,
        byteSize
	);
}

void DX12ConstantManager::InitializeSRV(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, UINT numConstants, UINT byteStirde)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_DX12ConstantUploader->GetResource()->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = numConstants;
    srvDesc.Buffer.StructureByteStride = byteStirde;
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

DX12MaterialConstantManager::DX12MaterialConstantManager()
{

}

DX12MaterialConstantManager::~DX12MaterialConstantManager()
{

}

void DX12ConstantManager::UploadConstant(ID3D12Device* device, DX12CommandList* dx12CommandList, UINT byteSize, const void* sourceAddress)
{
    m_DX12ConstantUploader->CreateUploadBuffer(device, byteSize);
    m_DX12ConstantUploader->CopyAndUploadResource(m_DX12ConstantUploader->GetUploadBuffer(), sourceAddress, byteSize);
    m_DX12ConstantUploader->TransitionState(dx12CommandList->GetCommandList(), D3D12_RESOURCE_STATE_COPY_DEST);
    dx12CommandList->GetCommandList()->CopyBufferRegion(m_DX12ConstantUploader->GetResource(), 0, m_DX12ConstantUploader->GetUploadBuffer(), 0, byteSize);
    m_DX12ConstantUploader->TransitionState(dx12CommandList->GetCommandList(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void DX12MaterialConstantManager::InitialzieUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT byteSize)
{
    if (m_materials.empty())
    {
        ::OutputDebugStringA("No Material was Loaded in Material Manager!");
        assert(m_materials.empty());
    }
    DX12ConstantManager::InitialzieUploadBuffer(device, cmdList, byteSize);
}

void DX12MaterialConstantManager::InitializeSRV(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, UINT numConstants, UINT byteStirde)
{
    if (m_materials.empty())
    {
        ::OutputDebugStringA("No Material was Loaded in Material Manager!");
        assert(m_materials.empty());
    }
    DX12ConstantManager::InitializeSRV(device, cpuHandle, numConstants, byteStirde);
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

DX12ObjectConstantManager::DX12ObjectConstantManager()
{

}

DX12ObjectConstantManager::~DX12ObjectConstantManager()
{

}

void DX12ObjectConstantManager::InitialzieUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT numConstants)
{
    if (m_objectConstants.empty())
    {
        ::OutputDebugStringA("No ObjectConstant was Loaded in ObjectConstant Manager!");
        assert(m_objectConstants.empty());
    }
    DX12ConstantManager::InitialzieUploadBuffer(device, cmdList, numConstants);
}

void DX12ObjectConstantManager::InitializeSRV(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, UINT numConstants, UINT byteStirde)
{
    if (m_objectConstants.empty())
    {
        ::OutputDebugStringA("No ObjectConstant was Loaded in ObjectConstant Manager!");
        assert(m_objectConstants.empty());
    }
    DX12ConstantManager::InitializeSRV(device, cpuHandle, numConstants, byteStirde);
}

void DX12ObjectConstantManager::PushObjectConstant(ObjectConstants Obj)
{
    m_objectConstants.emplace_back(Obj);
}

void DX12ObjectConstantManager::CreateSingleUploadBuffer(ID3D12Device* device, UINT totalBytes)
{
    m_regions.clear();
    m_cursor = 0;
    m_capacity = totalBytes;
    m_DX12ConstantUploader->CreateUploadBuffer(device, totalBytes);

    // Map
    void* mapped = nullptr;
    ThrowIfFailed(m_DX12ConstantUploader->GetUploadBuffer()->Map(0, nullptr, &mapped));
    m_mappedBase = reinterpret_cast<std::byte*>(mapped);
}

void DX12ObjectConstantManager::StageObjectConstants(const void* src, UINT byteSize, UINT dstOffset)
{
    memcpy(m_mappedBase + m_cursor, src, byteSize);

    ConstantCopyRegion r;
    r.srcOffset = m_cursor;
    r.dstOffset = dstOffset;
    r.byteSize = byteSize;
    m_regions.push_back(r);

    m_cursor += byteSize;
}

void DX12ObjectConstantManager::RecordObjectConstants(ID3D12GraphicsCommandList* commandList)
{
    m_DX12ConstantUploader->GetUploadBuffer()->Unmap(0, nullptr);
    m_DX12ConstantUploader->TransitionState(commandList, D3D12_RESOURCE_STATE_COPY_DEST);

    for (const auto& r : m_regions) 
    {
        commandList->CopyBufferRegion(
            m_DX12ConstantUploader->GetResource(),
            static_cast<UINT64>(r.dstOffset),
            m_DX12ConstantUploader->GetUploadBuffer(),
            static_cast<UINT64>(r.srcOffset),
            static_cast<UINT64>(r.byteSize));
    }

    m_DX12ConstantUploader->TransitionState(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    //reset
    m_mappedBase = nullptr;
    m_regions.clear();
    m_cursor = 0;
    m_capacity = 0;
}