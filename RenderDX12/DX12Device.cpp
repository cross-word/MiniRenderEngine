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

	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

	InitDX12RTVDescHeap();
	InitDX12DSVDescHeap();
	InitDX12SRVHeap();
	InitDX12ImGuiHeap();
	InitDX12FrameResource();
	InitDX12CommandList(m_DX12FrameResource[0]->GetCommandAllocator());
	CreateDX12FrameResourceSRV();
	InitDX12SwapChain(hWnd);
	InitDX12RootSignature();
	InitShader();
	InitDX12ShadowManager();

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
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
}

void DX12Device::InitDX12DSVDescHeap()
{
	m_DX12DSVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12DSVHeap->Initialize(
		m_device.Get(),
		EngineConfig::SwapChainBufferCount + EngineConfig::SwapChainBufferCount + 1, // back buffer + msaa buffer + 1 shadow map
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
}

void DX12Device::InitDX12SRVHeap()
{
	m_DX12SRVHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12SRVHeap->Initialize(
		m_device.Get(),
		EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount +
		2 * EngineConfig::MaxTextureCount
		+ 1
		+ 1
		+ 1, // 1 constant(b0) * 3 frames + 2 * texture amount + 1 material vectors + 1 world vectors + 1 shadow map
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
}

void DX12Device::InitDX12ImGuiHeap()
{
	m_DX12ImGuiHeap = std::make_unique<DX12DescriptorHeap>();
	m_DX12ImGuiHeap->Initialize(
		m_device.Get(),
		1, //  1 imgui
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
}

void DX12Device::InitDX12RootSignature()
{
	m_DX12RootSignature = std::make_unique<DX12RootSignature>();
	m_DX12RootSignature->Initialize(m_device.Get());
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

	std::string poolMax = std::to_string(EngineConfig::MaxTextureCount);
	std::string numLight = std::to_string(m_sceneData.lights.size() + 1);
	D3D_SHADER_MACRO macros[] =
	{
		{ "NUM_TEXTURE", poolMax.c_str() },
		{ "NUM_LIGHTS", numLight.c_str()},
		{ nullptr, nullptr }
	};

	hr = D3DCompileFromFile(EngineConfig::ShaderFilePath, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_5_1", compileFlags, 0, &m_vertexShader, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
	}
	hr = D3DCompileFromFile(EngineConfig::ShaderFilePath, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS", "ps_5_1", compileFlags, 0, &m_pixelShader, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
	}

	hr = D3DCompileFromFile(EngineConfig::ShadowShaderFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_5_1", compileFlags, 0, &m_shadowVertexShader, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
	}

	hr = D3DCompileFromFile(EngineConfig::ShadowShaderFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS", "ps_5_1", compileFlags, 0, &m_shadowPixelShader, &errorBlob);
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
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (UINT)offsetof(Vertex, tangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void DX12Device::CreateDX12PSO()
{
	m_DX12PSO = std::make_unique<DX12PSO>();
	//main render PSO
	m_DX12PSO->CreatePSO(
		GetDevice(),
		m_inputLayout,
		m_DX12RootSignature->GetRootSignature(),
		m_DX12SwapChain->GetDepthStencilFormat(),
		m_DX12SwapChain->GetRenderTargetFormat(),
		m_vertexShader.Get(),
		m_pixelShader.Get(),
		1);

	//shadow PSO
	m_DX12PSO->CreatePSO(
		GetDevice(),
		m_inputLayout,
		m_DX12RootSignature->GetRootSignature(),
		m_DX12ShadowManager->GetShadowDepthStencilFormat(),
		DXGI_FORMAT_UNKNOWN,
		m_shadowVertexShader.Get(),
		m_shadowPixelShader.Get(),
		0);
}

void DX12Device::PrepareInitialResource()
{
	m_DX12FrameResource[m_currBackBufferIndex]->ResetAllocator();
	m_DX12CommandList->ResetList(m_DX12FrameResource[m_currBackBufferIndex]->GetCommandAllocator());

	std::vector<TextureColorSpace> colorSpaces(m_sceneData.textures.size(), TextureColorSpace::Linear);
	for (const auto& mt : m_sceneData.materials)
	{
		if (mt.BaseColorIndex != UINT32_MAX && mt.BaseColorIndex < m_sceneData.textures.size())
			colorSpaces[mt.BaseColorIndex] = TextureColorSpace::SRGB;     // baseColor �� sRGB
		if (mt.EmissiveIndex != UINT32_MAX && mt.EmissiveIndex < m_sceneData.textures.size())
			colorSpaces[mt.EmissiveIndex] = TextureColorSpace::SRGB;     // emissive �� sRGB

		if (mt.NormalIndex != UINT32_MAX && mt.NormalIndex < m_sceneData.textures.size())
			colorSpaces[mt.NormalIndex] = TextureColorSpace::Linear;   // normal �� Linear
		if (mt.OcclusionIndex != UINT32_MAX && mt.OcclusionIndex < m_sceneData.textures.size())
			colorSpaces[mt.OcclusionIndex] = TextureColorSpace::Linear;   // AO �� Linear
		if (mt.ORMIndex != UINT32_MAX && mt.ORMIndex < m_sceneData.textures.size())
			colorSpaces[mt.ORMIndex] = TextureColorSpace::Linear;   // metallicRoughness �� Linear
	}

	// build texture SRV
	m_DX12TextureManager.clear();
	m_DX12TextureManager.reserve(m_sceneData.textures.size());
	for (size_t i = 0; i < m_sceneData.textures.size(); ++i)
	{
		auto SRVCpuHandle = m_DX12SRVHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + (UINT)i).cpuDescHandle;
		auto LinearCpuHandle = m_DX12SRVHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + EngineConfig::MaxTextureCount + (UINT)i).cpuDescHandle;

		auto tmpTexture = std::make_unique<DX12TextureManager>();
		std::string texName = "tex_" + std::to_string(i);
		tmpTexture->LoadAndCreateTextureResource(
			m_device.Get(),
			m_DX12CommandList.get(),
			&SRVCpuHandle,
			&LinearCpuHandle,
			m_sceneData.textures[i].c_str(),
			texName,
			colorSpaces[i]);
		m_DX12TextureManager.push_back(std::move(tmpTexture));
	}

	const uint32_t dummyStartIndex = m_sceneData.textures.size();
	for (uint32_t i = dummyStartIndex; i < EngineConfig::MaxTextureCount; ++i)
	{
		auto cpuHandle = m_DX12SRVHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + (UINT)i).cpuDescHandle;
		auto cpuHandle2 = m_DX12SRVHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + EngineConfig::MaxTextureCount + (UINT)i).cpuDescHandle;

		auto tmpTexture = std::make_unique<DX12TextureManager>();
		tmpTexture->CreateDummyTextureResource(
			m_device.Get(),
			m_DX12CommandList.get(),
			&cpuHandle);
		auto tmpTexture2 = std::make_unique<DX12TextureManager>();
		tmpTexture2->CreateDummyTextureResource(
			m_device.Get(),
			m_DX12CommandList.get(),
			&cpuHandle2);
		m_DX12TextureManager.push_back(std::move(tmpTexture));
		m_DX12TextureManager.push_back(std::move(tmpTexture2));
	}

	// build material SRV
	auto sanitize = [&](uint32_t idx)->uint32_t {
		return (idx == UINT32_MAX || idx >= EngineConfig::MaxTextureCount) ? 0u : idx;
		};

	auto mark = [&](UseFlags* useFlag, uint32_t idx, TextureColorSpace cs) {
		if (idx == UINT32_MAX || idx >= m_sceneData.textures.size()) return;
		if (cs == TextureColorSpace::SRGB) useFlag->SRGB = true; else useFlag->Linear = true;
		};

	m_DX12MaterialConstantManager = std::make_unique<DX12MaterialConstantManager>();
	for (size_t i = 0; i < m_sceneData.materials.size(); ++i)
	{
		auto tmpMaterial = std::make_unique<Material>();
		tmpMaterial->Name = "mat_" + std::to_string(i);
		tmpMaterial->MatCBIndex = (UINT)i;

		int baseIdx = m_sceneData.materials[i].BaseColorIndex >= 0 ? m_sceneData.materials[i].BaseColorIndex : 0;
		tmpMaterial->DiffuseSrvHeapIndex = (UINT)baseIdx;

		//mark(&(tmpMaterial->useFlag), m_sceneData.materials[i].BaseColorIndex, TextureColorSpace::SRGB);
		//mark(&(tmpMaterial->useFlag), m_sceneData.materials[i].EmissiveIndex, TextureColorSpace::SRGB);
		//mark(&(tmpMaterial->useFlag), m_sceneData.materials[i].NormalIndex, TextureColorSpace::Linear);
		//mark(&(tmpMaterial->useFlag), m_sceneData.materials[i].OcclusionIndex, TextureColorSpace::Linear);
		//mark(&(tmpMaterial->useFlag), m_sceneData.materials[i].ORMIndex, TextureColorSpace::Linear);

		MaterialConstants tmpMaterialConstant{};
		tmpMaterialConstant.DiffuseAlbedo = m_sceneData.materials[i].DiffuseAlbedo;
		tmpMaterialConstant.FresnelR0 = m_sceneData.materials[i].FresnelR0;
		tmpMaterialConstant.Roughness = m_sceneData.materials[i].Roughness;
		tmpMaterialConstant.Metallic = m_sceneData.materials[i].Metallic;
		tmpMaterialConstant.NormalScale = m_sceneData.materials[i].NormalScale;
		tmpMaterialConstant.OcclusionStrength = m_sceneData.materials[i].OcclusionStrength;
		tmpMaterialConstant.EmissiveStrength = m_sceneData.materials[i].EmissiveStrength;
		tmpMaterialConstant.EmissiveFactor = m_sceneData.materials[i].EmissiveFactor;
		tmpMaterialConstant.BaseColorIndex = sanitize(m_sceneData.materials[i].BaseColorIndex);
		tmpMaterialConstant.NormalIndex = sanitize(m_sceneData.materials[i].NormalIndex);
		tmpMaterialConstant.ORMIndex = sanitize(m_sceneData.materials[i].ORMIndex);
		tmpMaterialConstant.OcclusionIndex = sanitize(m_sceneData.materials[i].OcclusionIndex);
		tmpMaterialConstant.EmissiveIndex = sanitize(m_sceneData.materials[i].EmissiveIndex);

		tmpMaterial->matConstant = tmpMaterialConstant;

		m_DX12MaterialConstantManager->PushMaterial(std::move(tmpMaterial));
	}

	m_DX12MaterialConstantManager->InitialzieUploadBuffer(
		m_device.Get(),
		m_DX12CommandList->GetCommandList(),
		m_DX12MaterialConstantManager->GetMaterialCount() * sizeof(MaterialConstants));

	auto matCPUHandle = m_DX12SRVHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + 2 * EngineConfig::MaxTextureCount).cpuDescHandle;
	m_DX12MaterialConstantManager->InitializeSRV(
		m_device.Get(),
		&matCPUHandle,
		m_DX12MaterialConstantManager->GetMaterialCount(),
		sizeof(MaterialConstants));

	m_DX12MaterialConstantManager->UploadConstant(
		m_device.Get(),
		m_DX12CommandList.get(),
		m_DX12MaterialConstantManager->GetMaterialCount() * sizeof(MaterialConstants),
		m_DX12MaterialConstantManager->GetMaterialConstantData());

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

	auto MakeWorldInvTranspose = [&](const XMFLOAT4X4& world)->XMFLOAT4X4{
		XMMATRIX W = XMLoadFloat4x4(&world);
		W.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);
		XMMATRIX invT = XMMatrixTranspose(XMMatrixInverse(nullptr, W));
		XMFLOAT4X4 out;
		XMStoreFloat4x4(&out, invT);
		return out;
		};

	for (auto& instance : m_sceneData.instances)
	{
		Render::RenderItem renderItem{};
		XMFLOAT4X4 worldTranspose;
		renderItem.SetObjWorldMatrix(instance.world);
		renderItem.SetObjWorldInverseTransposeMatrix(MakeWorldInvTranspose(instance.world));
		renderItem.SetRenderGeometry(m_sceneGeometry[instance.primitive].get());
		UINT matIndex = (UINT)(m_sceneData.primitives[instance.primitive].material >= 0 ? m_sceneData.primitives[instance.primitive].material : 0);
		renderItem.SetMaterialIndex(matIndex);
		int tex = m_sceneData.materials[matIndex].BaseColorIndex; // baseColor��
		if (tex < 0 || tex >= (int)EngineConfig::MaxTextureCount) tex = 0; // ����Ʈ ȭ��Ʈ
		renderItem.SetTextureIndex((UINT)tex);
		m_renderItems.push_back(std::move(renderItem));
	}

	m_DX12ObjectConstantManager = std::make_unique<DX12ObjectConstantManager>();
	m_DX12ObjectConstantManager->InitialzieUploadBuffer(m_device.Get(), m_DX12CommandList->GetCommandList(), EngineConfig::NumDefaultObjectSRVSlot * sizeof(ObjectConstants));
	auto objCPUHandle = m_DX12SRVHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + 2 * EngineConfig::MaxTextureCount + 1).cpuDescHandle;
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

void DX12Device::CreateDX12FrameResourceSRV()
{
	for (int i = 0; i < EngineConfig::SwapChainBufferCount; i++)
	{
		m_DX12FrameResource[i]->CreateSRV(m_device.Get(), m_DX12SRVHeap.get(), i);
	}
}

void DX12Device::InitDX12ShadowManager()
{
	m_DX12ShadowManager = std::make_unique<DX12ShadowManager>();

	CD3DX12_CPU_DESCRIPTOR_HANDLE tmpDSVOffsetHandle = static_cast<CD3DX12_CPU_DESCRIPTOR_HANDLE>(GetOffsetCPUHandle(
			m_DX12DSVHeap->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
			2 * m_DX12SwapChain->GetSwapChainBufferCount(), // maind render + msaa render,
		m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE tmpRTVOffsetHandle = static_cast<CD3DX12_CPU_DESCRIPTOR_HANDLE>(m_DX12SRVHeap->Offset(
		EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount +
		2 * EngineConfig::MaxTextureCount
		+ 1
		+ 1).cpuDescHandle);

	m_DX12ShadowManager->Initialzie(m_device.Get(), m_DX12CommandList.get(), /**/ tmpDSVOffsetHandle, /*nullptr??*/ tmpRTVOffsetHandle);
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

	m_DX12FrameResource[m_currBackBufferIndex]->UploadPassConstant(m_camera.get(), m_sceneData.lights);
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