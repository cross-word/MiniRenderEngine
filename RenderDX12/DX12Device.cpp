#include "stdafx.h"
#include "DX12Device.h"
#include "D3DCamera.h"
#include "../FileLoader/SimpleLoader.h"

DX12Device::DX12Device()
{

}
DX12Device::~DX12Device()
{

}

void DX12Device::Initialize(HWND hWnd)
{
	assert(hWnd);

	// create hardware device
	// make hardware adaptor if it can. if not, make warp adapator.
	// *** require dx12 hardware ***
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)));

	HRESULT HardwareResult = D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(&m_device)
	);

	if (FAILED(HardwareResult))
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_device)
		));
	}
	InitDX12CommandList();
	InitDX12SwapChain(hWnd);
	InitDX12RTVDescHeap();
	InitDX12DSVDescHeap();
	InitDX12ConstantBufferDescHeap();
	InitDX12RootSignature();
	CreateDX12PSO();
}

void DX12Device::InitDX12CommandList()
{
	//create command queue/allocator/fence
	m_DX12CommandList = std::make_unique<DX12CommandList>();
	m_DX12CommandList->Initialize(m_device.Get());
}

void DX12Device::InitDX12SwapChain(HWND hWnd)
{
	//create swap chain
	m_DX12SwapChain = std::make_unique<DX12SwapChain>();
	m_DX12SwapChain->InitializeMultiSample(m_device.Get());
	m_DX12SwapChain->InitializeSwapChain(m_DX12CommandList.get(), m_factory.Get(), hWnd);
}

void DX12Device::InitDX12RTVDescHeap()
{
	m_DX12RTVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12RTVHeap->Initialize(
		m_device.Get(),
		m_DX12SwapChain->GetSwapChainBufferCount() + m_DX12SwapChain->GetSwapChainBufferCount(),
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);
}

void DX12Device::InitDX12DSVDescHeap()
{
	m_DX12DSVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12DSVHeap->Initialize(
		m_device.Get(),
		m_DX12SwapChain->GetSwapChainBufferCount() + m_DX12SwapChain->GetSwapChainBufferCount(),
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);
}

void DX12Device::InitDX12ConstantBufferDescHeap()
{
	m_DX12CBVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12CBVHeap->Initialize(
		m_device.Get(),
		3,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);
}

void DX12Device::InitDX12RootSignature()
{
	m_DX12RootSignature = std::make_unique<DX12RootSignature>();
	m_DX12RootSignature->Initialize(m_device.Get());
}

void DX12Device::CreateDX12PSO()
{
	m_DX12PSO = std::make_unique<DX12PSO>();
	InitShader();
	m_DX12PSO->CreatePSO(
		GetDevice(),
		m_inputLayout,
		m_DX12RootSignature->GetRootSignature(),
		m_DX12SwapChain->GetRenderTargetFormat(),
		m_vertexShader.Get(),
		m_pixelShader.Get());
}

void DX12Device::InitShader()
{

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	HRESULT hr = S_OK;
	ID3DBlob* errorBlob = nullptr;

	hr = D3DCompileFromFile(EngineConfig::ShaderFilePath, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &m_vertexShader, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
	}
	hr = D3DCompileFromFile(EngineConfig::ShaderFilePath, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &m_pixelShader, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
	}
	m_inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void DX12Device::PrepareInitialResource()
{
	InitConstantBuffer();
	InitMeshFromOBJ(EngineConfig::ModelObjFilePath);
}

void DX12Device::InitConstantBuffer()
{
	//CREATE BUFFER
	//ALLOCATE GPU ADDRESS
	UINT elementByteSize[3] = 
	{	CalcConstantBufferByteSize(sizeof(PassConstants)),
		CalcConstantBufferByteSize(sizeof(ObjectConstants)),
		CalcConstantBufferByteSize(sizeof(MaterialConstants)) };
	int CBIndex = 0;
	auto descCPUAddress = m_DX12CBVHeap->GetDescHeap()->GetCPUDescriptorHandleForHeapStart();

	//PassConstantBuffer
	m_DX12PassConstantBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12PassConstantBuffer->CreateConstantBuffer(m_device.Get(), elementByteSize[CBIndex]);
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_DX12PassConstantBuffer->GetResource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC passCBVDesc;
	passCBVDesc.BufferLocation = cbAddress;
	passCBVDesc.SizeInBytes = elementByteSize[CBIndex];

	m_DX12PassConstantBufferView = std::make_unique<DX12View>(
		m_device.Get(),
		EViewType::EConstantBufferView,
		m_DX12PassConstantBuffer.get(),
		descCPUAddress,
		&passCBVDesc);

	//index offset
	//desc address offset
	descCPUAddress.ptr += m_DX12CBVHeap->GetDescIncSize();
	CBIndex += 1;
	//Object Constant Buffer
	m_DX12ObjectConstantBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12ObjectConstantBuffer->CreateConstantBuffer(m_device.Get(), elementByteSize[CBIndex]);
	cbAddress = m_DX12ObjectConstantBuffer->GetResource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC objCBVDesc;
	objCBVDesc.BufferLocation = cbAddress;
	objCBVDesc.SizeInBytes = elementByteSize[CBIndex];

	m_DX12ObjectConstantBufferView = std::make_unique<DX12View>(
		m_device.Get(),
		EViewType::EConstantBufferView,
		m_DX12ObjectConstantBuffer.get(),
		descCPUAddress,
		&objCBVDesc);

	//index offset
	//desc address offset
	descCPUAddress.ptr += m_DX12CBVHeap->GetDescIncSize();
	CBIndex += 1;
	//Material Constant Buffer
	m_DX12MaterialConstantBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12MaterialConstantBuffer->CreateConstantBuffer(m_device.Get(), elementByteSize[CBIndex]);
	cbAddress = m_DX12MaterialConstantBuffer->GetResource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc;
	matCBVDesc.BufferLocation = cbAddress;
	matCBVDesc.SizeInBytes = elementByteSize[CBIndex];

	m_DX12MaterialConstantBufferView = std::make_unique<DX12View>(
		m_device.Get(),
		EViewType::EConstantBufferView,
		m_DX12MaterialConstantBuffer.get(),
		descCPUAddress,
		&matCBVDesc);
}

void DX12Device::UpdateConstantBuffer()
{
	ObjectConstants objConst;
	objConst.World = XMMatrixTranspose(XMMatrixIdentity() * m_camera->GetViewMatrix() * m_camera->GetProjectionMatrix(float(800/600))); //TMP CODE FOR TEST CBV TABLE
	m_DX12ObjectConstantBuffer->CopyAndUploadResource(
		m_DX12ObjectConstantBuffer->GetResource(),
		&objConst,
		sizeof(ObjectConstants));
}

void DX12Device::InitMeshFromOBJ(const std::wstring& filename)
{
	MeshData mesh = LoadOBJ(filename);
	if (mesh.vertices.empty() || mesh.indices.empty())
		return;

	D3D12_CPU_DESCRIPTOR_HANDLE nullHandle = {};
	const UINT vertexBufferSize = (UINT)mesh.vertices.size() * sizeof(Vertex);
	const UINT vertexStride = sizeof(Vertex);
	const UINT indexBufferSize = (UINT)mesh.indices.size() * sizeof(uint32_t);

	m_DX12CommandList->ResetAllocator();
	m_DX12CommandList->ResetList();

	m_DX12VertexBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12VertexBuffer->CreateVertexBuffer(m_device.Get(), mesh.vertices, m_DX12CommandList->GetCommandList());

	m_DX12IndexBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12IndexBuffer->CreateIndexBuffer(m_device.Get(), mesh.indices, m_DX12CommandList->GetCommandList());

	m_DX12CommandList->SubmitAndWait();
	m_DX12VertexBuffer->ResetUploadBuffer();
	m_DX12IndexBuffer->ResetUploadBuffer();

	m_DX12VertexView = std::make_unique<DX12View>(
		m_device.Get(),
		EViewType::EVertexView,
		m_DX12VertexBuffer.get(),
		nullHandle,
		m_DX12VertexBuffer.get()->GetGPUVAdress(),
		vertexBufferSize,
		vertexStride
	);

	m_DX12IndexView = std::make_unique<DX12View>(
		m_device.Get(),
		EViewType::EIndexView,
		m_DX12IndexBuffer.get(),
		nullHandle,
		m_DX12IndexBuffer.get()->GetGPUVAdress(),
		indexBufferSize,
		0U,
		m_indexFormat
	);
}