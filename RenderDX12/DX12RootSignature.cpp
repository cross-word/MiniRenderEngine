#include "stdafx.h"
#include "DX12RootSignature.h"
#include "D3DUtil.h"

DX12RootSignature::DX12RootSignature()
{

}

DX12RootSignature::~DX12RootSignature()
{

}

void DX12RootSignature::Initialize(ID3D12Device* device)
{
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER1 slotRootParameter[4];

	// Create a single descriptor table of CBVs.
	CD3DX12_DESCRIPTOR_RANGE1 cbvTable[2];
	cbvTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	cbvTable[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	//cbvTable[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);

	CD3DX12_DESCRIPTOR_RANGE1 srvTable[2]; //s0 texture, s1 materials
	srvTable[0].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		3,
		0,
		0
	);
	srvTable[1].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,
		0,
		1
	);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	slotRootParameter[0].InitAsDescriptorTable(2, cbvTable);
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable[0], D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[2].InitAsDescriptorTable(1, &srvTable[1], D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[3].InitAsConstants(1, 2, 0);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(4, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr;
	hr = D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &errorBlob);
	if (FAILED(hr))
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		errorBlob->Release();
	}
	ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

	return;
}

//instance function of SampleDesc from Frank Luna
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> DX12RootSignature::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}