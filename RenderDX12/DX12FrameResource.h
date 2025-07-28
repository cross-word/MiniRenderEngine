#pragma once

#include "stdafx.h"
#include "DX12Resource.h"
#include "DX12View.h"
#include "DX12DescriptorHeap.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// CPU가 한 프레임의 명령 목록들을 구축하는데 필요한 자원들을 대표하는 클래스  
// cmdAlloc,ConstantBuffers
struct DX12FrameResource
{
public:

    DX12FrameResource(ID3D12Device* device, DX12DescriptorHeap* cbvHeap);
    DX12FrameResource(const DX12FrameResource& rhs) = delete;
    DX12FrameResource& operator=(const DX12FrameResource& rhs) = delete;
    ~DX12FrameResource();

    inline ID3D12CommandAllocator* GetCommandAllocator() const noexcept { return m_commandAllocator.Get(); }
    inline UINT GetFenceValue() { return m_fence; }
    inline void SetFenceValue(UINT newFenceValue) { m_fence = newFenceValue; }
    inline DX12ResourceBuffer* GetDX12PassConstantBuffer() const noexcept { return m_DX12PassConstantBuffer.get(); }
    inline DX12View* GetDX12PassConstantBufferView() const noexcept { return m_DX12PassConstantBufferView.get(); }
    inline DX12ResourceBuffer* GetDX12ObjectConstantBuffer() const noexcept { return m_DX12ObjectConstantBuffer.get(); }
    inline DX12View* GetDX12ObjectConstantBufferView() const noexcept { return m_DX12ObjectConstantBufferView.get(); }
    inline DX12ResourceBuffer* GetDX12MaterialConstantBuffer() const noexcept { return m_DX12MaterialConstantBuffer.get(); }
    inline DX12View* GetDX12MaterialConstantBufferView() const noexcept { return m_DX12MaterialConstantBufferView.get(); }
    void ResetAllocator() { ThrowIfFailed(m_commandAllocator->Reset()); }

private:
    // 명령 할당자는 GPU가 명령들을 다 처리한 후 재설정해야한다.
    // 따라서 프레임마다 할당자가 필요하다.
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;

    // 상수 버퍼는 그것을 참조하는 명령들을 GPU가 전부 처리한 후 갱신해야한다.
    // 따라서 여러 명령 할당자를 쓰면 프레임마다 상수버퍼가 필요하다.
    // 효율을 위해 물체에 따라 변하는 상수와 변하지 않는 상수를 구분한다.
    std::unique_ptr<DX12ResourceBuffer> m_DX12PassConstantBuffer;
    std::unique_ptr<DX12View> m_DX12PassConstantBufferView;
    std::unique_ptr<DX12ResourceBuffer> m_DX12ObjectConstantBuffer;
    std::unique_ptr<DX12View> m_DX12ObjectConstantBufferView;
    std::unique_ptr<DX12ResourceBuffer> m_DX12MaterialConstantBuffer;
    std::unique_ptr<DX12View> m_DX12MaterialConstantBufferView;

    // Fence는 현재 울타리 지점까지의 명령들을 표시하는 값이다.
    // 이 값은 GPU가 이 프레임의 자원들을 사용하고 있는지 판정한다. 따라서 FrameResource에서 GPU의 명령 할당자를 바꾸는 척도가 된다.
    UINT64 m_fence = 0;
};