#include "stdafx.h"
#include "DX12TextureManager.h"
#include <comdef.h>

DX12TextureManager::DX12TextureManager()
{

}

DX12TextureManager::~DX12TextureManager()
{

}

void DX12TextureManager::LoadAndCreateTextureResource(
    ID3D12Device* device,
    DX12CommandList* dx12CommandList,
    const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
    const wchar_t* filename,
    const std::string textureName)
{
    //resource
    TexMetadata metaData;
    ScratchImage img;
    std::filesystem::path path(filename);
    HRESULT hr = S_OK;

    if (path.extension() == L".dds")
    {
        hr = LoadFromDDSFile(filename, DDS_FLAGS_NONE, &metaData, img);
    }
    else
    {
        hr = LoadFromWICFile(filename, WIC_FLAGS_FORCE_SRGB, &metaData, img);
    }
    if (FAILED(hr))
    {
        _com_error err(hr);
        std::wstringstream ss;
        ss << L"[Texture] Load failed: " << path.wstring()
            << L" hr=0x" << std::hex << hr
            << L" msg=" << err.ErrorMessage() << L"\n";
        OutputDebugStringW(ss.str().c_str());
        ThrowIfFailed(hr);
    }

    m_textureResource = std::make_unique<DX12ResourceTexture>();
    m_textureResource->CreateTexture(
        device,
        dx12CommandList,
        &metaData,
        &img);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_textureResource->GetResource()->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = m_textureResource->GetResource()->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    //view
    m_DX12TextureView = std::make_unique<DX12View>(
        device,
        EViewType::EShaderResourceView,
        m_textureResource.get(),
        *cpuHandle,
        nullptr,
        &srvDesc);

    m_fileName = filename;
    m_textureName = textureName;
}

void DX12TextureManager::CreateDummyTextureResource(
    ID3D12Device* device,
    DX12CommandList* dx12CommandList,
    const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle)
{
    TexMetadata meta;
    ScratchImage img;
    meta.width = 1; meta.height = 1;
    meta.arraySize = 1; meta.mipLevels = 1;
    meta.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    meta.dimension = TEX_DIMENSION_TEXTURE2D;
    ThrowIfFailed(img.Initialize2D(meta.format, meta.width, meta.height, 1, 1));

    m_textureResource = std::make_unique<DX12ResourceTexture>();
    m_textureResource->CreateTexture(
        device,
        dx12CommandList,
        &meta,
        &img
    );

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = meta.format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    //view
    m_DX12TextureView = std::make_unique<DX12View>(
        device,
        EViewType::EShaderResourceView,
        m_textureResource.get(),
        *cpuHandle,
        nullptr,
        &srvDesc);

    m_textureName = "DummyWhite";
}