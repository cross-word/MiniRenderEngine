#include "stdafx.h"
#include "DX12Device.h"
#include "D3DCamera.h"
#include "../FileLoader/SimpleLoader.h"

DX12Device::DX12Device()
{
	m_fenceEvent = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
}
DX12Device::~DX12Device()
{
	if (m_fenceEvent) CloseHandle(m_fenceEvent);
	m_DX12FrameResource.clear(); ////wt
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
	InitDX12RTVDescHeap();
	InitDX12DSVDescHeap();
	InitDX12CBVHeap();
	InitDX12SRVHeap();
	InitDX12FrameResource();
	InitDX12CommandList(m_DX12FrameResource[0]->GetCommandAllocator());
	InitDX12SwapChain(hWnd);
	InitDX12RootSignature();
	CreateDX12PSO();
}

void DX12Device::InitDX12CommandList(ID3D12CommandAllocator* commandAllocator)
{
	//create command queue/list/fence
	m_DX12CommandList = std::make_unique<DX12CommandList>();
	m_DX12CommandList->Initialize(m_device.Get(), commandAllocator);
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
		EngineConfig::SwapChainBufferCount + EngineConfig::SwapChainBufferCount, // back buffer + msaa buffer
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);
}

void DX12Device::InitDX12DSVDescHeap()
{
	m_DX12DSVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12DSVHeap->Initialize(
		m_device.Get(),
		EngineConfig::SwapChainBufferCount + EngineConfig::SwapChainBufferCount, // back buffer + msaa buffer
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);
}

void DX12Device::InitDX12CBVHeap()
{
	m_DX12CBVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12CBVHeap->Initialize(
		m_device.Get(),
		3* EngineConfig::SwapChainBufferCount, // 3 constant(b0 b1 b2) * 3 frames + 1 imgui
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);
}

void DX12Device::InitDX12SRVHeap()
{
	m_DX12SRVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12SRVHeap->Initialize(
		m_device.Get(),
		1, //  1 imgui
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

	hr = D3DCompileFromFile(EngineConfig::ShaderFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &m_vertexShader, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
	}
	hr = D3DCompileFromFile(EngineConfig::ShaderFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &m_pixelShader, &errorBlob);
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
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void DX12Device::PrepareInitialResource(UINT currentFenceValue)
{
	m_DX12RenderItem = std::make_unique<DX12RenderItem>();
	m_DX12RenderItem->InitMeshFromFile(
		m_device.Get(),
		m_DX12FrameResource[m_currBackBufferIndex].get(),
		m_DX12CommandList.get(),
		EngineConfig::ModelObjFilePath,
		currentFenceValue
	);
}

void DX12Device::InitDX12FrameResource()
{
	for (int i = 0; i < EngineConfig::SwapChainBufferCount; i++)
	{
		m_DX12FrameResource.push_back(std::make_unique<DX12FrameResource>(m_device.Get(), m_DX12CBVHeap.get(), i));
	}
}

void DX12Device::UpdateFrameResource()
{
	m_DX12CurrFrameResource = m_DX12FrameResource[m_currBackBufferIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (m_DX12CurrFrameResource->GetFenceValue() != 0 && m_DX12CommandList->GetFence()->GetCompletedValue() < m_DX12CurrFrameResource->GetFenceValue())
	{
		ThrowIfFailed(m_DX12CommandList->GetFence()->SetEventOnCompletion(
			m_DX12CurrFrameResource->GetFenceValue(), m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_DX12CurrFrameResource->UploadPassConstant();
	m_DX12CurrFrameResource->UploadObjectConstant(m_camera.get());
	m_DX12CurrFrameResource->UploadMaterialConstat();
}