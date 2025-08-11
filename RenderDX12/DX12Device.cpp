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
	m_DX12FrameResource.clear(); ////wt
}

void DX12Device::Initialize(HWND hWnd, const std::wstring& sceneFile)
{
	assert(hWnd);
	m_sceneData = LoadGLTFScene(sceneFile);

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

	HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	InitDX12RTVDescHeap();
	InitDX12DSVDescHeap();
	InitDX12SRVHeap(m_sceneData.textures.size());
	InitDX12ImGuiHeap();
	InitDX12FrameResource();
	InitDX12CommandList(m_DX12FrameResource[0]->GetCommandAllocator());
	InitDX12FrameResourceCBVRSV();
	InitDX12SwapChain(hWnd);
	InitDX12RootSignature(m_sceneData.textures.size());
	CreateDX12PSO();
}

void DX12Device::InitDX12CommandList(ID3D12CommandAllocator* commandAllocator)
{
	//create command queue/list/fence
	m_DX12CommandList = std::make_unique<DX12CommandList>();
	m_DX12CommandList->Initialize(m_device.Get(), commandAllocator, m_fenceEvent);
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

void DX12Device::InitDX12SRVHeap(size_t textureCount)
{
	m_DX12SRVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12SRVHeap->Initialize(
		m_device.Get(),
		EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + textureCount + 1 + 1, // 1 constant(b0) * 3 frames + texture amount + 1 material vectors + 1 world vectors
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);
}

void DX12Device::InitDX12ImGuiHeap()
{
	m_DX12ImGuiHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12ImGuiHeap->Initialize(
		m_device.Get(),
		1, //  1 imgui
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);
}

void DX12Device::InitDX12RootSignature(size_t textureCount)
{
	m_DX12RootSignature = std::make_unique<DX12RootSignature>();
	m_DX12RootSignature->Initialize(m_device.Get(), (UINT)textureCount);
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
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, normal),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, (UINT)offsetof(Vertex, texC),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, tangentU), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void DX12Device::PrepareInitialResource()
{
	m_DX12FrameResource[m_currBackBufferIndex]->ResetAllocator();
	m_DX12CommandList->ResetList(m_DX12FrameResource[m_currBackBufferIndex]->GetCommandAllocator());

	// build texture SRV
	m_DX12TextureManager.clear();
	m_DX12TextureManager.reserve(m_sceneData.textures.size());
	for (size_t i = 0; i < m_sceneData.textures.size(); ++i)
	{
		auto cpuHandle = m_DX12SRVHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + (UINT)i).cpuDescHandle;

		auto tmpTexture = std::make_unique<DX12TextureManager>();
		std::string texName = "tex_" + std::to_string(i);
		tmpTexture->LoadAndCreateTextureResource(
			m_device.Get(),
			m_DX12CommandList.get(),
			&cpuHandle,
			m_sceneData.textures[i].c_str(),
			texName
		);
		m_DX12TextureManager.push_back(std::move(tmpTexture));
	}

	// build material SRV
	m_DX12MaterialConstantManager = std::make_unique<DX12MaterialConstantManager>();
	for (size_t i = 0; i < m_sceneData.materials.size(); ++i)
	{
		auto tmpMaterial = std::make_unique<Material>();
		tmpMaterial->Name = "mat_" + std::to_string(i);
		tmpMaterial->MatCBIndex = (UINT)i;

		int baseIdx = m_sceneData.materials[i].baseColor >= 0 ? m_sceneData.materials[i].baseColor : 0;
		tmpMaterial->DiffuseSrvHeapIndex = (UINT)baseIdx;

		MaterialConstants tmpMaterialConstant{};
		tmpMaterialConstant.DiffuseAlbedo = m_sceneData.materials[i].baseColorFactor;
		tmpMaterialConstant.FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
		tmpMaterialConstant.Roughness = m_sceneData.materials[i].roughnessFactor;
		tmpMaterial->matConstant = tmpMaterialConstant;
		
		m_DX12MaterialConstantManager->PushMaterial(std::move(tmpMaterial));
	}

	m_DX12MaterialConstantManager->InitialzieUploadBuffer(
		m_device.Get(),
		m_DX12CommandList->GetCommandList(),
		m_DX12MaterialConstantManager->GetMaterialCount() * sizeof(MaterialConstants));

	auto matCPUHandle = m_DX12SRVHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + (UINT)m_sceneData.textures.size()).cpuDescHandle;
	m_DX12MaterialConstantManager->InitializeSRV(
		m_device.Get(),
		&matCPUHandle,
		m_DX12MaterialConstantManager->GetMaterialCount(),
		sizeof(MaterialConstants));

	// build Geometry
	m_sceneGeometry.clear();
	m_sceneGeometry.resize(m_sceneData.primitives.size());
	for (size_t p = 0; p < m_sceneData.primitives.size(); ++p)
	{
		auto tmpGeometry = std::make_unique<DX12RenderGeometry>();
		tmpGeometry->InitMeshFromData(
			m_device.Get(),
			m_DX12CommandList.get(),
			m_sceneData.primitives[p].mesh,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_sceneGeometry[p] = std::move(tmpGeometry);
	}

	// build RenderItems
	m_renderItems.clear();
	m_renderItems.reserve(m_sceneData.instances.size());
	for (auto& instance : m_sceneData.instances)
	{
		Render::RenderItem renderItem{};
		XMFLOAT4X4 worldTranspose;
		XMMATRIX world = XMLoadFloat4x4(&instance.world);
		XMStoreFloat4x4(&worldTranspose, XMMatrixTranspose(world));
		renderItem.SetObjWorldMatrix(worldTranspose);
		renderItem.SetRenderGeometry(m_sceneGeometry[instance.primitive].get());
		renderItem.SetMaterialIndex(m_sceneData.primitives[instance.primitive].material >= 0 ? m_sceneData.primitives[instance.primitive].material : 0);
		m_renderItems.push_back(std::move(renderItem));
	}

	m_DX12ObjectConstantManager = std::make_unique<DX12ObjectConstantManager>();
	m_DX12ObjectConstantManager->InitialzieUploadBuffer(m_device.Get(), m_DX12CommandList->GetCommandList(), EngineConfig::NumDefaultObjectSRVSlot * sizeof(ObjectConstants));
	auto objCPUHandle = m_DX12SRVHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + m_sceneData.textures.size() + 1).cpuDescHandle;
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
		m_DX12FrameResource[i]->CreateCBVSRV(m_device.Get(), m_DX12CommandList->GetCommandList(), m_DX12SRVHeap.get(), i);
	}
}

void DX12Device::UpdateFrameResource()
{
	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (m_DX12FrameResource[m_currBackBufferIndex]->GetFenceValue() != 0 && m_DX12CommandList->GetFence()->GetCompletedValue() < m_DX12FrameResource[m_currBackBufferIndex]->GetFenceValue())
	{
		ThrowIfFailed(m_DX12CommandList->GetFence()->SetEventOnCompletion(m_DX12FrameResource[m_currBackBufferIndex]->GetFenceValue(), m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_DX12FrameResource[m_currBackBufferIndex]->UploadPassConstant(m_camera.get());
	m_DX12FrameResource[m_currBackBufferIndex]->UploadObjectConstant(m_device.Get(), m_DX12CommandList.get(), m_renderItems, m_DX12ObjectConstantManager.get());
}

UINT DX12Device::GetTextureIndexAsTextureName(const std::string textureName)
{
	if (m_DX12TextureManager.empty())
	{
		::OutputDebugStringA("No Texture was Loaded in Texture Manager!");
		assert(false);
	}

	for (int i = 0; i < m_DX12TextureManager.size(); i++)
	{
		if (m_DX12TextureManager[i]->GetTextureName() == textureName) return i;
	}

	std::string msg = "Texture Name '" + textureName + "' was not found in DX12TextureManager. Automatically return 0.\n";
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