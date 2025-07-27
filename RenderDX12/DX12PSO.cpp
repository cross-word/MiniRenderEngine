#include "stdafx.h"
#include "DX12PSO.h"

DX12PSO::DX12PSO()
{

}

DX12PSO::~DX12PSO()
{

}

void DX12PSO::CreatePSO(
	ID3D12Device* m_device,
	const std::vector<D3D12_INPUT_ELEMENT_DESC>& m_inputLayout,
	ID3D12RootSignature* m_rootSignature,
	DXGI_FORMAT m_renderTargetFormat,
	ID3DBlob* m_vertexShader,
	ID3DBlob* m_pixelShader)
{
	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
	psoDesc.pRootSignature = m_rootSignature;
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vertexShader);
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_pixelShader);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = m_renderTargetFormat;
	psoDesc.SampleDesc.Count = 8;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.RasterizerState.MultisampleEnable = TRUE;

	// allocate PSO to vector
	ComPtr<ID3D12PipelineState> tmpPSO;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&tmpPSO)));
	m_pipelineStates.push_back(tmpPSO);

	return;
}

ID3D12PipelineState* DX12PSO::GetPipelineState()
{
	if (m_pipelineStates.empty()) return nullptr;

	return m_pipelineStates.front().Get();
}

ID3D12PipelineState* DX12PSO::GetPipelineState(int index)
{
	assert(index < m_pipelineStates.size());

	return m_pipelineStates[index].Get();
}