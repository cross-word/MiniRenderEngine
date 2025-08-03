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
	m_DX12CBVDDSHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12CBVDDSHeap->Initialize(
		m_device.Get(),
		EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + std::size(EngineConfig::DDSFilePath) + 1, // 2 constant(b0 b1) * 3 frames + dds amount + 1 material vectors
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);
}

void DX12Device::InitDX12SRVHeap()
{
	m_DX12ImGuiHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12ImGuiHeap->Initialize(
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
		m_DX12SwapChain->GetDepthStencilFormat(),
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

	hr = D3DCompileFromFile(EngineConfig::ShaderFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", compileFlags, 0, &m_vertexShader, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
	}
	hr = D3DCompileFromFile(EngineConfig::ShaderFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", compileFlags, 0, &m_pixelShader, &errorBlob);
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

void DX12Device::PrepareInitialResource()
{
	m_DX12CommandList->ResetList(m_DX12FrameResource[m_currBackBufferIndex]->GetCommandAllocator());
	int i = 0;
	for (auto ddsFileName : EngineConfig::DDSFilePath)
	{
		auto cpuHandle = m_DX12CBVDDSHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + i).cpuDescHandle;
		std::string texName = "texture" + std::to_string(i);
		auto ddsItem = std::make_unique<DX12DDSManager>();
		ddsItem->LoadAndCreateDDSResource(
			m_device.Get(),
			m_DX12CommandList->GetCommandList(),
			&cpuHandle,
			ddsFileName,
			texName
		);
		m_DX12DDSManager.emplace_back(std::move(ddsItem));
		i++;
	}
	m_DX12CommandList->SubmitAndWait(); //제출 너무 빈번함. 수정하는게 필요할듯? 멀티스레딩할때

	for (auto objFileName : EngineConfig::ModelObjFilePath)
	{
		auto geomeryItem = std::make_unique<DX12RenderGeometry>();
		if (geomeryItem->InitMeshFromFile(
			m_device.Get(),
			m_DX12FrameResource[m_currBackBufferIndex].get(),
			m_DX12CommandList.get(),
			objFileName
		))
		{
			m_DX12RenderGeometry.emplace_back(std::move(geomeryItem));
		}
	}

	////////////////////////////////////////////////////////
	///tmp code
	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	MaterialConstants brickConst;
	brickConst.DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	brickConst.FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	brickConst.Roughness = 0.1f;
	bricks0->matConstant = brickConst;

	auto stone0 = std::make_unique<Material>();
	stone0->Name = "stone0";
	stone0->MatCBIndex = 1;
	stone0->DiffuseSrvHeapIndex = 1;
	MaterialConstants stoneConst;
	stoneConst.DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stoneConst.FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	stoneConst.Roughness = 0.3f;
	stone0->matConstant = stoneConst;

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->MatCBIndex = 2;
	tile0->DiffuseSrvHeapIndex = 2;
	MaterialConstants tileConst;
	tileConst.DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tileConst.FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tileConst.Roughness = 0.3f;
	tile0->matConstant = tileConst;

	m_DX12MaterialManager = std::make_unique<DX12MaterialManager>();

	m_DX12MaterialManager->PushMaterialVector(std::move(bricks0));
	m_DX12MaterialManager->PushMaterialVector(std::move(stone0));
	m_DX12MaterialManager->PushMaterialVector(std::move(tile0));
	m_DX12MaterialManager->InitialzieUploadBuffer(m_device.Get(), m_DX12CommandList->GetCommandList());
	auto matCPUHandle = m_DX12CBVDDSHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + std::size(EngineConfig::DDSFilePath)).cpuDescHandle;
	m_DX12MaterialManager->InitializeSRV(m_device.Get(), &matCPUHandle);
	m_DX12FrameResource[m_currBackBufferIndex]->ResetAllocator();
	m_DX12CommandList->ResetList(m_DX12FrameResource[m_currBackBufferIndex]->GetCommandAllocator());

	m_DX12MaterialManager->UploadMaterial(m_device.Get(), m_DX12CommandList.get());

	RenderItem newRenderItem;
	newRenderItem.SetRenderGeometry(m_DX12RenderGeometry[0].get());
	newRenderItem.SetTextureIndex(GetTextureIndexAsTextureName("texture1"));
	newRenderItem.SetMaterialIndex(GetMaterialIndexAsMaterialName("stone0"));
	newRenderItem.SetBaseVertexLocation(0);
	newRenderItem.SetStartIndexLocation(0);
	m_renderItems.push_back(newRenderItem);

	RenderItem newRenderItem1;
	newRenderItem1.SetRenderGeometry(m_DX12RenderGeometry[1].get());
	newRenderItem1.SetTextureIndex(GetTextureIndexAsTextureName("texture0"));
	newRenderItem1.SetMaterialIndex(GetMaterialIndexAsMaterialName("tile0"));
	newRenderItem1.SetBaseVertexLocation(0);
	newRenderItem1.SetStartIndexLocation(0);
	m_renderItems.push_back(newRenderItem1);

	RenderItem newRenderItem2;
	newRenderItem2.SetRenderGeometry(m_DX12RenderGeometry[0].get());
	newRenderItem2.SetTextureIndex(GetTextureIndexAsTextureName("texture2"));
	newRenderItem2.SetMaterialIndex(GetMaterialIndexAsMaterialName("bricks0"));
	newRenderItem2.SetBaseVertexLocation(0);
	newRenderItem2.SetStartIndexLocation(0);
	m_renderItems.push_back(newRenderItem2);
	////////////////////////////////////////////////////////
}

void DX12Device::InitDX12FrameResource()
{
	for (int i = 0; i < EngineConfig::SwapChainBufferCount; i++)
	{
		m_DX12FrameResource.push_back(std::move(std::make_unique<DX12FrameResource>(m_device.Get(), m_DX12CBVDDSHeap.get(), i)));
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
	//m_DX12CurrFrameResource->UploadMaterialConstat();
}

UINT DX12Device::GetTextureIndexAsTextureName(const std::string textureName)
{
	if (m_DX12DDSManager.empty())
	{
		::OutputDebugStringA("No Texture was Loaded in DDS Manager!");
		assert(false);
	}

	for (int i = 0; i < m_DX12DDSManager.size(); i++)
	{
		if (m_DX12DDSManager[i]->GetDDSTextureName() == textureName) return i;
	}

	std::string msg = "Texture Name '" + textureName + "' was not found in DX12DDSManager. Automatically return 0.\n";
	::OutputDebugStringA(msg.c_str());

	return 0;
}

UINT DX12Device::GetMaterialIndexAsMaterialName(const std::string materialName)
{
	if (m_DX12MaterialManager->IsMaterailEmpty())
	{
		::OutputDebugStringA("No Material was Loaded in Material Manager!");
		assert(false);
	}

	for (int i = 0; i < m_DX12MaterialManager->GetMaterialCount(); i++)
	{
		if (m_DX12MaterialManager->GetMaterial(i)->Name == materialName) return i;
	}

	std::string msg = "Material Name '" + materialName + "' was not found in DX12MaterialManager. Automatically return 0.\n";
	::OutputDebugStringA(msg.c_str());

	return 0;
}