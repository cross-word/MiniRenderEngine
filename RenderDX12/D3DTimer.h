#pragma once
#include "stdafx.h"

class D3DTimer
{
public:
    void Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, UINT frameCount);
    void Begin(ID3D12GraphicsCommandList* cmdList, UINT frameIndex);
    void End(ID3D12GraphicsCommandList* cmdList, UINT frameIndex);
    float GetElapsedMS(UINT frameIndex);

private:
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_readbackBuffer;
    UINT64 m_frequency = 0;
    UINT m_frameCount = 0;
};