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
	InitDX12CBVDDSHeap();
	InitDX12SRVHeap();
	InitDX12FrameResource();
	InitDX12CommandList(m_DX12FrameResource[0]->GetCommandAllocator());
	InitDX12FrameResourceCBVRSV();
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

void DX12Device::InitDX12CBVDDSHeap()
{
	m_DX12CBVDDSHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12CBVDDSHeap->Initialize(
		m_device.Get(),
		EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + std::size(EngineConfig::DDSFilePath) + 1 + 1, // 1 constant(b0) * 3 frames + dds amount + 1 material vectors + 1 world vectors
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
	m_DX12FrameResource[m_currBackBufferIndex]->ResetAllocator();
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

	for (auto objFileName : EngineConfig::ModelObjFilePath)
	{
		auto geometryItem = std::make_unique<DX12RenderGeometry>();
		if (geometryItem->InitMeshFromFile(
			m_device.Get(),
			m_DX12CommandList.get(),
			objFileName
		))
		{
			m_DX12RenderGeometry.emplace_back(std::move(geometryItem));
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

	m_DX12MaterialConstantManager = std::make_unique<DX12MaterialConstantManager>();

	m_DX12MaterialConstantManager->PushMaterial(std::move(bricks0));
	m_DX12MaterialConstantManager->PushMaterial(std::move(stone0));
	m_DX12MaterialConstantManager->PushMaterial(std::move(tile0));
	m_DX12MaterialConstantManager->InitialzieUploadBuffer(m_device.Get(), m_DX12CommandList->GetCommandList(), m_DX12MaterialConstantManager->GetMaterialCount() * sizeof(MaterialConstants));
	auto matCPUHandle = m_DX12CBVDDSHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + std::size(EngineConfig::DDSFilePath)).cpuDescHandle;
	m_DX12MaterialConstantManager->InitializeSRV(m_device.Get(), &matCPUHandle, m_DX12MaterialConstantManager->GetMaterialCount(), sizeof(MaterialConstants));
	m_DX12MaterialConstantManager->UploadConstant(
		m_device.Get(), 
		m_DX12CommandList.get(),
		m_DX12MaterialConstantManager->GetMaterialCount() * sizeof(MaterialConstants),
		m_DX12MaterialConstantManager->GetMaterialConstantData());

	Render::RenderItem newRenderItem;
	newRenderItem.SetRenderGeometry(m_DX12RenderGeometry[0].get());
	newRenderItem.SetTextureIndex(GetTextureIndexAsTextureName("texture1"));
	newRenderItem.SetMaterialIndex(GetMaterialIndexAsMaterialName("stone0"));
	newRenderItem.SetBaseVertexLocation(0);
	newRenderItem.SetStartIndexLocation(0);
	XMFLOAT4X4 Wmat;
	XMFLOAT4X4 Tmat;
	XMMATRIX W = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + 2 * 5.0f);
	XMStoreFloat4x4(&Wmat, XMMatrixTranspose(W));
	newRenderItem.SetObjWorldMatrix(Wmat);
	XMMATRIX T = XMMatrixTranslation(-50.0f, 10.5f, -10.0f + 2 * 50.0f);
	XMStoreFloat4x4(&Tmat, XMMatrixTranspose(T));
	newRenderItem.SetObjTransformMatrix(Tmat);
	m_renderItems.push_back(newRenderItem);

	Render::RenderItem newRenderItem1;
	newRenderItem1.SetRenderGeometry(m_DX12RenderGeometry[1].get());
	newRenderItem1.SetTextureIndex(GetTextureIndexAsTextureName("texture0"));
	newRenderItem1.SetMaterialIndex(GetMaterialIndexAsMaterialName("tile0"));
	newRenderItem1.SetBaseVertexLocation(0);
	newRenderItem1.SetStartIndexLocation(0);
	XMFLOAT4X4 Wmat2;
	XMFLOAT4X4 Tmat2;
	W = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + 1 * 5.0f);
	XMStoreFloat4x4(&Wmat2, XMMatrixTranspose(W));
	newRenderItem1.SetObjWorldMatrix(Wmat2);
	T = XMMatrixTranslation(-50.0f, 10.5f, -10.0f + 1 * 50.0f);
	XMStoreFloat4x4(&Tmat2, XMMatrixTranspose(T));
	newRenderItem1.SetObjTransformMatrix(Tmat2);
	m_renderItems.push_back(newRenderItem1);

	Render::RenderItem newRenderItem2;
	newRenderItem2.SetRenderGeometry(m_DX12RenderGeometry[1].get());
	newRenderItem2.SetTextureIndex(GetTextureIndexAsTextureName("texture2"));
	newRenderItem2.SetMaterialIndex(GetMaterialIndexAsMaterialName("bricks0"));
	newRenderItem2.SetBaseVertexLocation(0);
	newRenderItem2.SetStartIndexLocation(0);
	XMFLOAT4X4 Wmat3;
	XMFLOAT4X4 Tmat3;
	W = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + 3 * 5.0f);
	XMStoreFloat4x4(&Wmat3, XMMatrixTranspose(W));
	newRenderItem2.SetObjWorldMatrix(Wmat3);
	T = XMMatrixTranslation(-50.0f, 10.5f, -10.0f + 3 * 50.0f);
	XMStoreFloat4x4(&Tmat3, XMMatrixTranspose(T));
	newRenderItem2.SetObjTransformMatrix(Tmat3);
	m_renderItems.push_back(newRenderItem2);
	////////////////////////////////////////////////////////
	/////////////
	auto base = EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount;
	auto texBase = base;
	auto matSlot = base + std::size(EngineConfig::DDSFilePath);
	auto worldSlot = matSlot + 1;

	m_DX12ObjectConstantManager = std::make_unique<DX12ObjectConstantManager>();
	m_DX12ObjectConstantManager->InitialzieUploadBuffer(m_device.Get(), m_DX12CommandList->GetCommandList(), EngineConfig::NumDefaultObjectSRVSlot * sizeof(ObjectConstants));
	auto objCPUHandle = m_DX12CBVDDSHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + std::size(EngineConfig::DDSFilePath) + 1).cpuDescHandle;
	m_DX12ObjectConstantManager->InitializeSRV(m_device.Get(), &objCPUHandle, EngineConfig::NumDefaultObjectSRVSlot, sizeof(ObjectConstants));

	//wait for upload and reset upload buffers
	m_DX12CommandList->SubmitAndWait();
	for (int i = 0; i < m_DX12RenderGeometry.size(); i++)
	{
		m_DX12RenderGeometry[i]->GetDX12VertexBuffer()->ResetUploadBuffer();
		m_DX12RenderGeometry[i]->GetDX12IndexBuffer()->ResetUploadBuffer();
	}
	m_DX12MaterialConstantManager->GetMaterialResource()->ResetUploadBuffer();
	/////////////
}

void DX12Device::InitDX12FrameResource()
{
	for (int i = 0; i < EngineConfig::SwapChainBufferCount; i++)
	{
		m_DX12FrameResource.push_back(std::move(std::make_unique<DX12FrameResource>(m_device.Get())));
	}
}

void DX12Device::InitDX12FrameResourceCBVRSV()
{
	for (int i = 0; i < EngineConfig::SwapChainBufferCount; i++)
	{
		m_DX12FrameResource[i]->CreateCBVSRV(m_device.Get(), m_DX12CommandList->GetCommandList(), m_DX12CBVDDSHeap.get(), i);
	}
}

void DX12Device::UpdateFrameResource()
{
	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (m_DX12FrameResource[m_currBackBufferIndex]->GetFenceValue() != 0 && m_DX12CommandList->GetFence()->GetCompletedValue() < m_DX12FrameResource[m_currBackBufferIndex]->GetFenceValue())
	{
		ThrowIfFailed(m_DX12CommandList->GetFence()->SetEventOnCompletion(
			m_DX12FrameResource[m_currBackBufferIndex]->GetFenceValue(), m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_DX12FrameResource[m_currBackBufferIndex]->UploadPassConstant(m_camera.get());
	m_DX12FrameResource[m_currBackBufferIndex]->ResetAllocator();
	m_DX12CommandList->ResetList(m_DX12FrameResource[m_currBackBufferIndex]->GetCommandAllocator());
	m_DX12FrameResource[m_currBackBufferIndex]->UploadObjectConstant(m_device.Get(), m_DX12CommandList->GetCommandList(), m_renderItems, m_DX12ObjectConstantManager.get());
	m_DX12CommandList->SubmitAndWait();
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
	if (m_DX12MaterialConstantManager->IsMaterailEmpty())
	{
		::OutputDebugStringA("No Material was Loaded in Material Manager!");
		assert(false);
	}

	for (int i = 0; i < m_DX12MaterialConstantManager->GetMaterialCount(); i++)
	{
		if (m_DX12MaterialConstantManager->GetMaterial(i)->Name == materialName) return i;
	}

	std::string msg = "Material Name '" + materialName + "' was not found in DX12MaterialManager. Automatically return 0.\n";
	::OutputDebugStringA(msg.c_str());

	return 0;
}