#pragma once

#include "stdafx.h"
#include <DirectXTex.h>
#include <filesystem>

#include "D3DUtil.h"
#include "DX12Resource.h"
#include "DX12View.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class DX12TextureManager
{
public:
    DX12TextureManager();
    ~DX12TextureManager();

    void LoadAndCreateTextureResource(
        ID3D12Device* device,
        DX12CommandList* DX12CommandList,
        const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
        const wchar_t* filename,
        const std::string textureName);
    inline DX12ResourceTexture* GetTextureResource() const noexcept { return m_textureResource.get(); }
    inline DX12View* GetDX12TextureView() const noexcept { return m_DX12TextureView.get(); }
    inline std::string GetTextureName() const noexcept { return m_textureName; }
    inline std::wstring GetFileName() const noexcept { return m_fileName; }
private:
    std::unique_ptr<DX12ResourceTexture> m_textureResource;
    std::unique_ptr<DX12View> m_DX12TextureView;

    std::string m_textureName;
    std::wstring m_fileName;
};