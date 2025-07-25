#include "stdafx.h"
#include "DX12Device.h"

DX12Device::DX12Device()
{

}
DX12Device::~DX12Device()
{

}

void DX12Device::Initialize(HWND hWnd)
{
	assert(hWnd);
	UINT dxgiFactoryFlags = 0;

	//enable debug layer
#if defined(DEBUG) || defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

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

	m_DX12swapChain = std::make_unique<DX12SwapChain>();

	InitDX12CommandList();
	//create swap chain
	m_DX12swapChain = std::make_unique<DX12SwapChain>();
	m_DX12swapChain->IntializeMultiSample(m_device.Get());
	m_DX12swapChain->InitializeSwapChain(m_DX12CommandList.get(), m_factory.Get(), hWnd);

	//create RTV/DSV descriptor heap

	m_DX12rtvHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12rtvHeap->Initialize(
		m_device.Get(),
		m_DX12swapChain->GetSwapChainBufferCount(),
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	m_DX12dsvHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12dsvHeap->Initialize(
		m_device.Get(),
		1,
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	////////////////////////////////
	// MAKING DEVICE END
	////////////////////////////////

	//Constant Buffer Desc Heap
	m_DX12cbvHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12cbvHeap->Initialize(
		m_device.Get(),
		1,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);

	//Init Root Signature
	InitDX12RootSignature();
	CreateDX12PSO();
}

void DX12Device::InitDX12CommandList()
{
	//create command queue/allocator/fence
	m_DX12CommandList = std::make_unique<DX12CommandList>();
	m_DX12CommandList->Initialize(m_device.Get());
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
	m_DX12PSO->CreatePSO(m_device.Get(), m_inputLayout, m_DX12RootSignature->GetRootSignature(), m_vertexShader.Get(), m_pixelShader.Get());
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

	hr = D3DCompileFromFile(L"..\\Shaders\\color.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &m_vertexShader, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
	}
	hr = D3DCompileFromFile(L"..\\Shaders\\color.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &m_pixelShader, &errorBlob);
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
	CreateDX12PSO();
	InitConstantBuffer();
	InitVertex();//tmp func
}

void DX12Device::InitConstantBuffer()
{
	//create constant buffer resource
	m_DX12constantBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12constantBuffer->CreateConstantBuffer(m_device.Get());

	//allocate cbv
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_DX12constantBuffer->GetResource()->GetGPUVirtualAddress();
	UINT elementByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));

	int CBufIndex = 0;
	cbAddress += CBufIndex * elementByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = CalcConstantBufferByteSize(sizeof(ObjectConstants));

	m_DX12constantBufferView = std::make_unique<DX12View>(
		m_device.Get(),
		EViewType::EConstantBufferView,
		m_DX12constantBuffer.get(),
		m_DX12cbvHeap->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
		&cbvDesc
	);
}

void DX12Device::UpdateConstantBuffer()
{
	//copy constant value to constant buffer (temp code)
	XMFLOAT4X4 objConstants;
	XMStoreFloat4x4(&objConstants, XMMatrixIdentity());
	m_DX12constantBuffer->UploadConstantBuffer(&objConstants, sizeof(objConstants));
}

//Should recieve vertex(or verteces)
void DX12Device::InitVertex()
{
	Vertex triangleVertices[] =
	{
		{ { 0.0f, 0.25f , 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.25f, -0.25f , 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.25f, -0.25f , 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
	};
	D3D12_CPU_DESCRIPTOR_HANDLE nullHandle = {};
	const UINT vertexBufferSize = sizeof(triangleVertices);
	const UINT vertexStride = sizeof(Vertex);
	m_DX12vertexBuffer = std::make_unique<DX12ResourceBuffer>();
	//make vertex res
	//make vertex view
	m_DX12vertexBuffer->CreateVertexBuffer(m_device.Get(), triangleVertices, vertexBufferSize);

	// Copy the triangle data to the vertex buffer.(tmp code)
	CD3DX12_RANGE readRange(0, 0);
	m_DX12vertexBuffer->UploadConstantBuffer(readRange, triangleVertices, vertexBufferSize);

	m_DX12vertexView = std::make_unique<DX12View>(
		m_device.Get(),
		EViewType::EVertexView,
		m_DX12vertexBuffer.get(),
		nullHandle,
		m_DX12vertexBuffer.get()->GetGPUVAdress(),
		vertexBufferSize,
		vertexStride
	);
}