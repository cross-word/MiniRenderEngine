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

	m_DX12SwapChain = std::make_unique<DX12SwapChain>();

	InitDX12CommandList();
	//create swap chain
	m_DX12SwapChain = std::make_unique<DX12SwapChain>();
	m_DX12SwapChain->IntializeMultiSample(m_device.Get());
	m_DX12SwapChain->InitializeSwapChain(m_DX12CommandList.get(), m_factory.Get(), hWnd);

	//create RTV/DSV descriptor heap

	m_DX12RTVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12RTVHeap->Initialize(
		m_device.Get(),
		m_DX12SwapChain->GetSwapChainBufferCount()+1,
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	m_DX12DSVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12DSVHeap->Initialize(
		m_device.Get(),
		1+1,
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	////////////////////////////////
	// MAKING DEVICE END
	////////////////////////////////

	//Constant Buffer Desc Heap
	m_DX12CBVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12CBVHeap->Initialize(
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
	InitIndex();//tmp func
}

void DX12Device::InitConstantBuffer()
{
	//create constant buffer resource
	m_DX12ConstantBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12ConstantBuffer->CreateConstantBuffer(m_device.Get());

	//allocate cbv
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_DX12ConstantBuffer->GetResource()->GetGPUVirtualAddress();
	UINT elementByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants::WorldViewProj));

	int CBufIndex = 0;
	cbAddress += CBufIndex * elementByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = CalcConstantBufferByteSize(sizeof(ObjectConstants::WorldViewProj));

	m_DX12ConstantBufferView = std::make_unique<DX12View>(
		m_device.Get(),
		EViewType::EConstantBufferView,
		m_DX12ConstantBuffer.get(),
		m_DX12CBVHeap->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
		&cbvDesc
	);
}

void DX12Device::UpdateConstantBuffer()
{
	//copy constant value to constant buffer (temp code)
	XMFLOAT4X4 objConstants;
	ObjectConstants obj;
	XMStoreFloat4x4(&objConstants, obj.WorldViewProj);
	m_DX12ConstantBuffer->CopyAndUploadResource(m_DX12ConstantBuffer->GetResource(), &objConstants, sizeof(ObjectConstants::WorldViewProj));
}

//Should receive vertex(or verteces)
void DX12Device::InitVertex()
{

	std::array<Vertex, 8> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	D3D12_CPU_DESCRIPTOR_HANDLE nullHandle = {};
	const UINT vertexBufferSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT vertexStride = sizeof(Vertex);

	m_DX12CommandList->ResetAllocator();
	m_DX12CommandList->ResetList();

	m_DX12VertexBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12VertexBuffer->CreateVertexBuffer(m_device.Get(), vertices, m_DX12CommandList->GetCommandList());

	m_DX12CommandList->SubmitAndWait(); //임시방편

	m_DX12VertexView = std::make_unique<DX12View>(
		m_device.Get(),
		EViewType::EVertexView,
		m_DX12VertexBuffer.get(),
		nullHandle,
		m_DX12VertexBuffer.get()->GetGPUVAdress(),
		vertexBufferSize,
		vertexStride
	);
}

//should receive index
void DX12Device::InitIndex()
{
	std::array<std::uint32_t, 36> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	};

	D3D12_CPU_DESCRIPTOR_HANDLE nullHandle = {};
	const UINT indexBufferSize = (UINT)indices.size() * sizeof(uint32_t);

	m_DX12CommandList->ResetAllocator();
	m_DX12CommandList->ResetList();

	m_DX12IndexBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12IndexBuffer->CreateIndexBuffer(m_device.Get(), indices, m_DX12CommandList->GetCommandList());

	m_DX12CommandList->SubmitAndWait(); //임시방편

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