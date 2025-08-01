#pragma once

#include "stdafx.h"
#include <DirectXTex.h>

#include "D3DUtil.h"
#include "DX12Resource.h"
#include "DX12View.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class DX12DDSManager
{
public:
    DX12DDSManager();
    ~DX12DDSManager();

    void LoadAndCreateDDSResource(
        ID3D12Device* device, 
        ID3D12GraphicsCommandList* cmdList, 
        const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, 
        const wchar_t* filename);
    inline DX12ResourceTexture* GetDDSResource() const noexcept { return m_DDSResource.get(); }
    inline DX12View* GetDX12DDSView() const noexcept { return m_DX12DDSView.get(); }
private:
    std::unique_ptr<DX12ResourceTexture> m_DDSResource;
    std::unique_ptr<DX12View> m_DX12DDSView;
};