#pragma once

#include "stdafx.h"
#include <DirectXTex.h>
#include <filesystem>

#include "D3DUtil.h"
#include "DX12Resource.h"
#include "DX12View.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

enum class TextureColorSpace { Linear, SRGB };

class DX12TextureManager
{
public:
    DX12TextureManager();
    ~DX12TextureManager();

    void LoadAndCreateTextureResource(
        ID3D12Device* device,
        DX12CommandList* dx12CommandList,
        const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
        const D3D12_CPU_DESCRIPTOR_HANDLE* LinearCpuHandle,
        const wchar_t* filename,
        const std::string textureName,
        TextureColorSpace colorSpace);
    void CreateDummyTextureResource(
        ID3D12Device* device,
        DX12CommandList* dx12CommandList,
        const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle);
    void LoadAndCreateCubeTextureResource(
        ID3D12Device* device,
        DX12CommandList* dx12CommandList,
        const D3D12_CPU_DESCRIPTOR_HANDLE* SRGBCpuHandle,
        const wchar_t* filename,
        const std::string textureName);

    inline DX12ResourceTexture* GetTextureResource() const noexcept { return m_textureResource.get(); }
    inline DX12View* GetDX12SRGBTextureView() const noexcept { return m_DX12SRGBTextureView.get(); }
    inline DX12View* GetDX12LinearTextureView() const noexcept { return m_DX12LinearTextureView.get(); }
    inline std::string GetTextureName() const noexcept { return m_textureName; }
    inline std::wstring GetFileName() const noexcept { return m_fileName; }
private:
    std::unique_ptr<DX12ResourceTexture> m_textureResource;
    std::unique_ptr<DX12View> m_DX12SRGBTextureView;
    std::unique_ptr<DX12View> m_DX12LinearTextureView;

    std::string m_textureName;
    std::wstring m_fileName;
};