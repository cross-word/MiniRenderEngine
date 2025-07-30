#include "stdafx.h"
#include "RenderDX12.h"

#include <d3d12sdklayers.h>

#include "../external/imgui/imgui.h"
#include "../external/imgui/backends/imgui_impl_win32.h"
#include "../external/imgui/backends/imgui_impl_dx12.h"

namespace {
	UINT64 fenceCounter = 0;
};

//struct for imgui (see : https://github.com/ocornut/imgui/blob/master/examples/example_win32_directx12/main.cpp#L113 )
struct ExampleDescriptorHeapAllocator
{
	ID3D12DescriptorHeap* Heap = nullptr;
	D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
	D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
	UINT                        HeapHandleIncrement;
	ImVector<int>               FreeIndices;

	void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
	{
		IM_ASSERT(Heap == nullptr && FreeIndices.empty());
		Heap = heap;
		D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
		HeapType = desc.Type;
		HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
		HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
		HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
		FreeIndices.reserve((int)desc.NumDescriptors);
		for (int n = desc.NumDescriptors; n > 0; n--)
			FreeIndices.push_back(n - 1);
	}
	void Destroy()
	{
		Heap = nullptr;
		FreeIndices.clear();
	}
	void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
	{
		IM_ASSERT(FreeIndices.Size > 0);
		int idx = FreeIndices.back();
		FreeIndices.pop_back();
		out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
		out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
	}
	void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
	{
		int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
		int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
		IM_ASSERT(cpu_idx == gpu_idx);
		FreeIndices.push_back(cpu_idx);
	}
};

static ExampleDescriptorHeapAllocator g_SrvAllocator;

RenderDX12::RenderDX12()
{

}

RenderDX12::~RenderDX12()
{
	ShutDown();
}

void RenderDX12::InitializeDX12(HWND hWnd)
{
	assert(hWnd);
	UINT dxgiFactoryFlags = 0;

//enable debug layer
#if defined(DEBUG) || defined(_DEBUG)
{
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController))))
	{
		m_debugController->EnableDebugLayer();

		// Enable additional debug layers.
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
}
#endif
	m_DX12Device.Initialize(hWnd);
	m_DX12FrameBuffer.Initialize(&m_DX12Device);
	m_timer.Initialize(m_DX12Device.GetDevice(), m_DX12Device.GetDX12CommandList()->GetCommandQueue(), EngineConfig::SwapChainBufferCount);
	//////////////////////////////////////////
	// imgui initialization block
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();

	float uiScale = 0.9f;
	io.FontGlobalScale = uiScale;
	style.ScaleAllSizes(uiScale);

	ImGui_ImplWin32_Init(hWnd);

	g_SrvAllocator.Create(m_DX12Device.GetDevice(), m_DX12Device.GetDX12CBVHeap()->GetDescHeap());

	ImGui_ImplDX12_InitInfo info = {};
	info.Device = m_DX12Device.GetDevice();
	info.CommandQueue = m_DX12Device.GetDX12CommandList()->GetCommandQueue();
	info.NumFramesInFlight = EngineConfig::SwapChainBufferCount;
	info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	info.DSVFormat = DXGI_FORMAT_UNKNOWN;
	info.SrvDescriptorHeap = m_DX12Device.GetDX12CBVHeap()->GetDescHeap();
	info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* outCPU, D3D12_GPU_DESCRIPTOR_HANDLE* outGPU) {
		return g_SrvAllocator.Alloc(outCPU, outGPU);
		};
	info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
		return g_SrvAllocator.Free(cpu, gpu);
		};
	ImGui_ImplDX12_Init(&info);
	ImGui_ImplDX12_CreateDeviceObjects();

	//////////////////////////////////////////
	m_DX12Device.GetDX12CommandList()->ResetList(
		m_DX12Device.GetFrameResource(m_DX12Device.GetCurrentBackBufferIndex())->GetCommandAllocator());
	m_DX12Device.GetDX12CommandList()->SubmitAndWait(++fenceCounter);
	OnResize();
	m_DX12Device.PrepareInitialResource(++fenceCounter);
}

void RenderDX12::OnResize()
{
	assert(m_DX12Device.GetDevice());
	assert(m_DX12Device.GetDX12SwapChain());
	for(int i = 0; i < EngineConfig::SwapChainBufferCount; i++) assert(m_DX12Device.GetFrameResource(i)->GetCommandAllocator());

	// Flush before changing any resources.
	m_DX12Device.GetDX12CommandList()->FlushCommandQueue(++fenceCounter);
	m_DX12Device.GetDX12CommandList()->ResetList(m_DX12Device.GetFrameResource(m_DX12Device.GetCurrentBackBufferIndex())->GetCommandAllocator());

	m_DX12FrameBuffer.Resize(&m_DX12Device, ++fenceCounter);
}

void RenderDX12::Draw()
{
	m_DX12Device.SetCurrentBackBufferIndex(m_DX12Device.GetDX12SwapChain()->GetSwapChain()->GetCurrentBackBufferIndex());
	UINT currBackBufferIndex = m_DX12Device.GetDX12SwapChain()->GetSwapChain()->GetCurrentBackBufferIndex();
	m_DX12Device.UpdateFrameResource();

	//timer set
	float gpuMS = m_timer.GetElapsedGPUMS(currBackBufferIndex);
	float cpuMS = m_timer.GetElapsedCPUMS(currBackBufferIndex);
	m_timer.BeginCPU(currBackBufferIndex);

	m_DX12Device.GetFrameResource(currBackBufferIndex)->ResetAllocator();
	m_DX12Device.GetDX12CommandList()->ResetList(m_DX12Device.GetDX12PSO()->GetPipelineState(), m_DX12Device.GetFrameResource(currBackBufferIndex)->GetCommandAllocator()); //tmpcode

	m_timer.BeginGPU(m_DX12Device.GetDX12CommandList()->GetCommandList(), currBackBufferIndex);
	m_DX12FrameBuffer.BeginFrame(&m_DX12Device, currBackBufferIndex);

	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootSignature(m_DX12Device.GetDX12RootSignature()->GetRootSignature());
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_DX12Device.GetDX12CBVHeap()->GetDescHeap() };

	auto descGPUAddress = m_DX12Device.GetDX12CBVHeap()->GetDescHeap()->GetGPUDescriptorHandleForHeapStart();
	descGPUAddress.ptr += SIZE_T(currBackBufferIndex) * EngineConfig::SwapChainBufferCount * m_DX12Device.GetDX12CBVHeap()->GetDescIncSize();
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootDescriptorTable(0, descGPUAddress);

	m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetVertexBuffers(0, 1, m_DX12Device.GetDX12VertexBufferView()->GetVertexBufferView());
	m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetIndexBuffer(m_DX12Device.GetDX12IndexBufferView()->GetIndexBufferView());
	m_DX12Device.GetDX12CommandList()->GetCommandList()->DrawIndexedInstanced(36, 1, 0, 0, 0);

	m_DX12FrameBuffer.EndFrame(&m_DX12Device, currBackBufferIndex);
	//////////////////////////////////////////////////////////////////////////////////
	// imgui render block
	m_timer.EndGPU(m_DX12Device.GetDX12CommandList()->GetCommandList(), currBackBufferIndex);
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Frame Times");
	ImGui::Text("CPU: %.3f ms \nGPU: %.3f ms", cpuMS, gpuMS);
	ImGui::End();
	ImGui::Render();

	//current bind to MSAA render target view (see func : DX12FrameBuffer::EndFrame)
	//rebinding to back buffer is needed to draw IMGUI on real screen!
	auto backBufferBase = m_DX12Device.GetDX12RTVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart();
	auto backBufferInc = m_DX12Device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE backbufRTV(backBufferBase, currBackBufferIndex, backBufferInc);

	m_DX12Device.GetDX12CommandList()->GetCommandList()->OMSetRenderTargets(1, &backbufRTV, FALSE, nullptr);
	ID3D12DescriptorHeap* heaps[] = { m_DX12Device.GetDX12CBVHeap()->GetDescHeap() };
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetDescriptorHeaps(1, heaps);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_DX12Device.GetDX12CommandList()->GetCommandList());
	m_DX12FrameBuffer.SetRTVPresent(&m_DX12Device, currBackBufferIndex);
	//////////////////////////////////////////////////////////////////////////////////

	m_DX12Device.GetDX12CommandList()->GetCommandList()->Close();
	ID3D12CommandList* lists[] = { m_DX12Device.GetDX12CommandList()->GetCommandList() };
	m_DX12Device.GetDX12CommandList()->GetCommandQueue()->ExecuteCommandLists(1, lists);

	// 프레임 펜스 기록(대기는 다음 프레임 Begin에서만)
	const UINT64 fenceValue = ++fenceCounter;
	m_DX12Device.GetDX12CommandList()->GetCommandQueue()->Signal(m_DX12Device.GetDX12CommandList()->GetFence(), fenceValue);
	m_DX12Device.GetFrameResource(currBackBufferIndex)->SetFenceValue(fenceValue);

	m_timer.EndCPU(currBackBufferIndex);
	m_DX12FrameBuffer.Present(&m_DX12Device);
}

void RenderDX12::ShutDown()
{
	// Cleanup
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	m_DX12Device.GetDX12CommandList()->FlushCommandQueue(++fenceCounter);
}