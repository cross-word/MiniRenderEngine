#include "stdafx.h"
#include "D3DTimer.h"

using Microsoft::WRL::ComPtr;

void D3DTimer::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, UINT frameCount)
{
    m_frameCount = frameCount;

    D3D12_QUERY_HEAP_DESC heapDesc = {};
    heapDesc.Count = frameCount * 2;
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_queryHeap));

    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * heapDesc.Count);
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&m_readbackBuffer));

    queue->GetTimestampFrequency(&m_frequency);
}

void D3DTimer::Begin(ID3D12GraphicsCommandList* cmdList, UINT frameIndex)
{
    cmdList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, frameIndex * 2);
}

void D3DTimer::End(ID3D12GraphicsCommandList* cmdList, UINT frameIndex)
{
    cmdList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, frameIndex * 2 + 1);
    cmdList->ResolveQueryData(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
        frameIndex * 2, 2, m_readbackBuffer.Get(),
        frameIndex * sizeof(UINT64) * 2);
}

float D3DTimer::GetElapsedMS(UINT frameIndex)
{
    UINT64* data = nullptr;
    D3D12_RANGE range{ frameIndex * 2 * sizeof(UINT64), (frameIndex * 2 + 2) * sizeof(UINT64) };
    if (SUCCEEDED(m_readbackBuffer->Map(0, &range, reinterpret_cast<void**>(&data))))
    {
        UINT64 start = data[frameIndex * 2];
        UINT64 end = data[frameIndex * 2 + 1];
        D3D12_RANGE empty{ 0,0 };
        m_readbackBuffer->Unmap(0, &empty);
        if (end > start && m_frequency)
        {
            return float(end - start) / static_cast<float>(m_frequency) * 1000.0f;
        }
    }
    return 0.0f;
}