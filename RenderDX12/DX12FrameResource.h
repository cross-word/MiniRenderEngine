#pragma once

#include "stdafx.h"
#include "DX12Resource.h"
#include "DX12View.h"
#include "DX12DescriptorHeap.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// CPU�� �� �������� ��� ��ϵ��� �����ϴµ� �ʿ��� �ڿ����� ��ǥ�ϴ� Ŭ����  
// cmdAlloc,ConstantBuffers
struct DX12FrameResource
{
public:

    DX12FrameResource(ID3D12Device* device, DX12DescriptorHeap* cbvHeap, uint32_t frameIndex);
    DX12FrameResource(const DX12FrameResource& rhs) = delete;
    DX12FrameResource& operator=(const DX12FrameResource& rhs) = delete;
    ~DX12FrameResource();

    inline ID3D12CommandAllocator* GetCommandAllocator() const noexcept { return m_commandAllocator.Get(); }
    inline UINT64 GetFenceValue() { return m_fence; }
    inline void SetFenceValue(uint64_t newFenceValue) { m_fence = newFenceValue; }
    inline DX12ResourceBuffer* GetDX12PassConstantBuffer() const noexcept { return m_DX12PassConstantBuffer.get(); }
    inline DX12View* GetDX12PassConstantBufferView() const noexcept { return m_DX12PassConstantBufferView.get(); }
    inline DX12ResourceBuffer* GetDX12ObjectConstantBuffer() const noexcept { return m_DX12ObjectConstantBuffer.get(); }
    inline DX12View* GetDX12ObjectConstantBufferView() const noexcept { return m_DX12ObjectConstantBufferView.get(); }
    inline DX12ResourceBuffer* GetDX12MaterialConstantBuffer() const noexcept { return m_DX12MaterialConstantBuffer.get(); }
    inline DX12View* GetDX12MaterialConstantBufferView() const noexcept { return m_DX12MaterialConstantBufferView.get(); }
    void ResetAllocator() { ThrowIfFailed(m_commandAllocator->Reset()); }

    //prepare for multi-thread
    void EnsureWorkerCapacity(ID3D12Device* device, uint32_t workerCount);
    ID3D12CommandAllocator* GetWorkerAllocator(uint32_t i) { return m_workerAlloc[i].Get(); }

private:
    // ��� �Ҵ��ڴ� GPU�� ��ɵ��� �� ó���� �� �缳���ؾ��Ѵ�.
    // ���� �����Ӹ��� �Ҵ��ڰ� �ʿ��ϴ�.
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;

    // ��� ���۴� �װ��� �����ϴ� ��ɵ��� GPU�� ���� ó���� �� �����ؾ��Ѵ�.
    // ���� ���� ��� �Ҵ��ڸ� ���� �����Ӹ��� ������۰� �ʿ��ϴ�.
    // ȿ���� ���� ��ü�� ���� ���ϴ� ����� ������ �ʴ� ����� �����Ѵ�.
    std::unique_ptr<DX12ResourceBuffer> m_DX12PassConstantBuffer; //b0
    std::unique_ptr<DX12View> m_DX12PassConstantBufferView;
    std::unique_ptr<DX12ResourceBuffer> m_DX12ObjectConstantBuffer; //b1
    std::unique_ptr<DX12View> m_DX12ObjectConstantBufferView;
    std::unique_ptr<DX12ResourceBuffer> m_DX12MaterialConstantBuffer; //b2
    std::unique_ptr<DX12View> m_DX12MaterialConstantBufferView;

    // Fence�� ���� ��Ÿ�� ���������� ��ɵ��� ǥ���ϴ� ���̴�.
    // �� ���� GPU�� �� �������� �ڿ����� ����ϰ� �ִ��� �����Ѵ�. ���� FrameResource���� GPU�� ��� �Ҵ��ڸ� �ٲٴ� ô���� �ȴ�.
    uint64_t m_fence = 0;

    std::vector<ComPtr<ID3D12CommandAllocator>> m_workerAlloc;
};