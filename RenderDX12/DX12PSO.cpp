#include "stdafx.h"
#include "DX12PSO.h"

DX12PSO::DX12PSO()
{

}

DX12PSO::~DX12PSO()
{

}

void DX12PSO::CreatePSO(
	ID3D12Device* device,
	const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout,
	ID3D12RootSignature* rootSignature,
	DXGI_FORMAT depthStencilFormat,
	DXGI_FORMAT renderTargetFormat,
	ID3DBlob* vertexShader,
	ID3DBlob* pixelShader,
	UINT numRenderTarget)
{
	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	psoDesc.pRootSignature = rootSignature;
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DSVFormat = depthStencilFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = numRenderTarget;
	psoDesc.RTVFormats[0] = renderTargetFormat;
	psoDesc.SampleDesc.Count = EngineConfig::MsaaSampleCount;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.RasterizerState.MultisampleEnable = TRUE;

	// allocate PSO to vector
	ComPtr<ID3D12PipelineState> tmpPSO;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&tmpPSO)));
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