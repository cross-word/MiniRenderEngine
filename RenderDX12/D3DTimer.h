#pragma once
#include "stdafx.h"

/*
CLASS D3DTIMER
MAIN WORK:
1. measure CPU Time in rendering pipeline (working time except waiting for GPU work)
2. measure GPU Time in rendering pipeline (working time except waiting for CPU work)
IMPORTANT MEMBER:
*/
class D3DTimer
{
public:
    void Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, UINT frameCount);
    // GPU timing -----------------------------------------------------------
    void BeginGPU(ID3D12GraphicsCommandList* cmdList, UINT frameIndex);
    void EndGPU(ID3D12GraphicsCommandList* cmdList, UINT frameIndex);
    float GetElapsedGPUMS(UINT frameIndex);

    // CPU timing -----------------------------------------------------------
    void BeginCPU(UINT frameIndex);
    void EndCPU(UINT frameIndex);
    float GetElapsedCPUMS(UINT frameIndex) const;

private:
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_readbackBuffer;
    UINT64 m_frequency = 0;
    UINT m_frameCount = 0;

    std::vector<LARGE_INTEGER> m_cpuBegin;
    std::vector<float>        m_cpuElapsedMS;
    LARGE_INTEGER             m_cpuFreq{};
};