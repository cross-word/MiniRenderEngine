#pragma once

#include "stdafx.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12PSO
MAIN WORK:
1. manage PSOs
IMPORTANT MEMBER:
1. m_pipelineStates
*/
class DX12PSO
{
public:
	DX12PSO();
	~DX12PSO();
	void CreatePSO(ID3D12Device* m_device, const std::vector<D3D12_INPUT_ELEMENT_DESC>& m_inputLayout, ID3D12RootSignature* m_rootSignature, ID3DBlob* m_vertexShader, ID3DBlob* m_pixelShader);
	ID3D12PipelineState* GetPipelineState();
	ID3D12PipelineState* GetPipelineState(int index);
private:
	std::vector<ComPtr<ID3D12PipelineState>> m_pipelineStates;
};