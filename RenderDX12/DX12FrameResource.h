#pragma once

#include "stdafx.h"
#include "DX12Resource.h"
#include "DX12View.h"
#include "DX12DescriptorHeap.h"
#include "D3DCamera.h"
#include "DX12ConstantManager.h"
#include "DX12RenderGeometry.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
namespace Render { struct RenderItem; }

// CPU�� �� �������� ��� ��ϵ��� �����ϴµ� �ʿ��� �ڿ����� ��ǥ�ϴ� Ŭ����  
// cmdAlloc,ConstantBuffers
struct DX12FrameResource
{
public:
    DX12FrameResource(ID3D12Device* device);
    DX12FrameResource(const DX12FrameResource& rhs) = delete;
    DX12FrameResource& operator=(const DX12FrameResource& rhs) = delete;
    ~DX12FrameResource();

    inline ID3D12CommandAllocator* GetCommandAllocator() const noexcept { return m_commandAllocator.Get(); }
    inline UINT64 GetFenceValue() { return m_fence; }
    inline void SetFenceValue(uint64_t newFenceValue) { m_fence = newFenceValue; }
    inline DX12ResourceBuffer* GetDX12PassConstantBuffer() const noexcept { return m_DX12PassConstantBuffer.get(); }
    inline DX12View* GetDX12PassConstantBufferView() const noexcept { return m_DX12PassConstantBufferView.get(); }
    void ResetAllocator() { ThrowIfFailed(m_commandAllocator->Reset()); }
    void CreateCBVSRV(ID3D12Device* device, ID3D12GraphicsCommandList* m_commandList, DX12DescriptorHeap* cbvHeap, uint32_t frameIndex);

    //prepare for multi-thread
    void EnsureWorkerCapacity(ID3D12Device* device, uint32_t workerCount);
    ID3D12CommandAllocator* GetWorkerAllocator(uint32_t i) { return m_workerAlloc[i].Get(); }

    void UpdatePassConstant();
    void UploadPassConstant(D3DCamera* d3dCamera);
    void UploadObjectConstant(
        ID3D12Device* device,
        DX12CommandList* DX12CommandList,
        DX12DescriptorHeap* DX12CBVDDSHeap,
        std::vector<Render::RenderItem>& renderItems,
        DX12ObjectConstantManager* DX12ObjectConstantManager);

private:
    // ��� �Ҵ��ڴ� GPU�� ��ɵ��� �� ó���� �� �缳���ؾ��Ѵ�.
    // ���� �����Ӹ��� �Ҵ��ڰ� �ʿ��ϴ�.
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;

    // ��� ���۴� �װ��� �����ϴ� ��ɵ��� GPU�� ���� ó���� �� �����ؾ��Ѵ�.
    // ���� ���� ��� �Ҵ��ڸ� ���� �����Ӹ��� ������۰� �ʿ��ϴ�.
    // ȿ���� ���� ��ü�� ���� ���ϴ� ����� ������ �ʴ� ����� �����Ѵ�.
    std::unique_ptr<DX12ResourceBuffer> m_DX12PassConstantBuffer; //b0
    std::unique_ptr<DX12View> m_DX12PassConstantBufferView;

    PassConstants m_passConstant;

    // Fence�� ���� ��Ÿ�� ���������� ��ɵ��� ǥ���ϴ� ���̴�.
    // �� ���� GPU�� �� �������� �ڿ����� ����ϰ� �ִ��� �����Ѵ�. ���� FrameResource���� GPU�� ��� �Ҵ��ڸ� �ٲٴ� ô���� �ȴ�.
    uint64_t m_fence = 0;

    std::vector<ComPtr<ID3D12CommandAllocator>> m_workerAlloc;
};