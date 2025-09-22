#include "stdafx.h"
#include "RenderDX12.h"

#include <d3d12sdklayers.h>
#include <pix3.h>
#include <algorithm>

#include "../external/imgui/imgui.h"
#include "../external/imgui/backends/imgui_impl_win32.h"
#include "../external/imgui/backends/imgui_impl_dx12.h"


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
	m_DX12Device.Initialize(hWnd, EngineConfig::SceneFilePath);
	m_DX12FrameBuffer.Initialize(&m_DX12Device);
	m_timer.Initialize(m_DX12Device.GetDevice(), m_DX12Device.GetDX12CommandList()->GetCommandQueue());
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

	g_SrvAllocator.Create(m_DX12Device.GetDevice(), m_DX12Device.GetDX12ImGuiHeap()->GetDescHeap());

	ImGui_ImplDX12_InitInfo info = {};
	info.Device = m_DX12Device.GetDevice();
	info.CommandQueue = m_DX12Device.GetDX12CommandList()->GetCommandQueue();
	info.NumFramesInFlight = EngineConfig::SwapChainBufferCount;
	info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	info.DSVFormat = DXGI_FORMAT_UNKNOWN;
	info.SrvDescriptorHeap = m_DX12Device.GetDX12ImGuiHeap()->GetDescHeap();
	info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* outCPU, D3D12_GPU_DESCRIPTOR_HANDLE* outGPU) {
		return g_SrvAllocator.Alloc(outCPU, outGPU);
		};
	info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
		return g_SrvAllocator.Free(cpu, gpu);
		};
	ImGui_ImplDX12_Init(&info);
	ImGui_ImplDX12_CreateDeviceObjects();

	//////////////////////////////////////////
	OnResize();
	m_DX12Device.PrepareInitialResource();
}

void RenderDX12::OnResize()
{
	assert(m_DX12Device.GetDevice());
	assert(m_DX12Device.GetDX12SwapChain());
	for (int i = 0; i < EngineConfig::SwapChainBufferCount; i++) assert(m_DX12Device.GetFrameResource(i)->GetCommandAllocator());

	// Flush before changing any resources.
	m_DX12Device.GetDX12CommandList()->FlushCommandQueue();
	m_DX12Device.GetDX12CommandList()->ResetList(m_DX12Device.GetFrameResource(m_DX12Device.GetCurrentBackBufferIndex())->GetCommandAllocator());

	m_DX12FrameBuffer.Resize(&m_DX12Device);
}

void RenderDX12::AllocateWorkerDrawingCommand(uint32_t i)
{
	while (true)
	{
		WorkerJobDrawing job;

		{//mutex block
			std::unique_lock<std::mutex> lock(m_workerMutex);
			m_workerCV.wait(lock, [&] { return m_terminateWorkers || m_workerJobDrawing[i].ready; }); //wait until notify(call from main thread per frame), prevent spurious wakeup
			if (m_terminateWorkers) break;
			job = m_workerJobDrawing[i];
		}

		//draw commands
		const uint32_t beginIndex = job.beginIndex;
		const uint32_t endIndex = job.endIndex;
		const uint32_t currBackBufferIndex = job.currBackBufferIndex;
		if (beginIndex < endIndex)
		{
			m_DX12Device.GetWorkerDX12CommandList(i)->ResetList(
				m_DX12Device.GetDX12PSO()->GetPipelineState(),
				m_DX12Device.GetFrameResource(currBackBufferIndex)->GetWorkerCommandAllocator(i));
			m_DX12FrameBuffer.SetRenderViewPort(m_DX12Device.GetWorkerDX12CommandList(i), currBackBufferIndex);
			m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList()->SetGraphicsRootSignature(m_DX12Device.GetDX12RootSignature()->GetRootSignature());
			ID3D12DescriptorHeap* descriptorHeaps[] = { m_DX12Device.GetDX12CBVSRVHeap()->GetDescHeap() };
			m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList()->SetGraphicsRootDescriptorTable(0, job.cbvSlice.gpuDescHandle);
			m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList()->SetGraphicsRootDescriptorTable(1, job.texSlice.gpuDescHandle);
			m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList()->SetGraphicsRootDescriptorTable(2, job.matSlice.gpuDescHandle);
			PIXBeginEvent(m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList(), PIX_COLOR(0, 128, 255), L"[MT] Draw worker %d (%d~%d)", i, beginIndex, endIndex);
			for (uint32_t renderItemIndex = beginIndex; renderItemIndex < endIndex; renderItemIndex++)
			{
				UINT matIndex = m_DX12Device.GetDX12RenderItem(renderItemIndex).GetMaterialIndex();
				UINT texIndex = m_DX12Device.GetDX12RenderItem(renderItemIndex).GetTextureIndex();
				UINT objIndex = m_DX12Device.GetDX12RenderItem(renderItemIndex).GetObjectConstantIndex();
				struct RootPush { UINT matIdx; UINT texIdx; UINT worldIdx; } push{ matIndex, texIndex, objIndex };
				m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList()->SetGraphicsRoot32BitConstants(3, 3, &push, 0);
				m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList()->IASetPrimitiveTopology(m_DX12Device.GetDX12RenderItem(renderItemIndex).GetRenderGeometry()->GetPrimitiveTopologyType());
				m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList()->IASetVertexBuffers(0, 1, m_DX12Device.GetDX12RenderItem(renderItemIndex).GetRenderGeometry()->GetDX12VertexBufferView()->GetVertexBufferView());
				m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList()->IASetIndexBuffer(m_DX12Device.GetDX12RenderItem(renderItemIndex).GetRenderGeometry()->GetDX12IndexBufferView()->GetIndexBufferView());
				m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList()->DrawIndexedInstanced(
					m_DX12Device.GetDX12RenderItem(renderItemIndex).GetRenderGeometry()->GetIndexCount(),
					1,
					m_DX12Device.GetDX12RenderItem(renderItemIndex).GetStartIndexLocation(),
					m_DX12Device.GetDX12RenderItem(renderItemIndex).GetBaseVertexLocation(),
					0);
			}
			PIXEndEvent(m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList());
			m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList()->Close();
		}

		{//mutex block
			std::lock_guard<std::mutex> lock(m_workerMutex);
			m_workerJobDrawing[i].ready = false;
			if (--m_pendingWorkers == 0) m_workerDoneCV.notify_one(); //wake up main thread
		}
	}
}

void RenderDX12::Draw()
{
	if (EngineConfig::UseMultiThread && workerCount > 0)
		RecordAndSubmit_Multi();
	else
		RecordAndSubmit_Single();
}

void RenderDX12::RecordAndSubmit_Single()
{
	PIXBeginEvent(m_DX12Device.GetDX12CommandList()->GetCommandQueue(), PIX_COLOR(0, 255, 0), L"Frame %u", sFrameId); //pix marking ~ total frame

	m_DX12Device.SetCurrentBackBufferIndex(m_DX12Device.GetDX12SwapChain()->GetSwapChain()->GetCurrentBackBufferIndex());
	uint32_t currBackBufferIndex = m_DX12Device.GetDX12SwapChain()->GetSwapChain()->GetCurrentBackBufferIndex();

	m_DX12FrameBuffer.CheckFence(&m_DX12Device, currBackBufferIndex); // << GPU WAIT
	////////////////imgui timer set
	float gpuMS = m_timer.GetElapsedGPUMS(currBackBufferIndex);
	float cpuMS = m_timer.GetElapsedCPUMS(currBackBufferIndex);
	m_DX12Device.GetFrameResource(currBackBufferIndex)->ResetAllocator();
	m_DX12Device.GetDX12CommandList()->ResetList(m_DX12Device.GetDX12PSO()->GetPipelineState(), m_DX12Device.GetFrameResource(currBackBufferIndex)->GetCommandAllocator());

	m_timer.BeginCPU(currBackBufferIndex); // << imgui CPU TIMER START

	PIXBeginEvent(m_DX12Device.GetDX12CommandList()->GetCommandList(), PIX_COLOR(0, 180, 255), L"BeginFrame"); // pix marking ~ frame start setting

	m_timer.BeginGPU(m_DX12Device.GetDX12CommandList()->GetCommandList(), currBackBufferIndex); // << imgui GPU TIMER START

	m_DX12Device.UpdateFrameResource();


	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootSignature(m_DX12Device.GetDX12RootSignature()->GetRootSignature());
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_DX12Device.GetDX12CBVSRVHeap()->GetDescHeap() };

	auto* cbvSrvHeap = m_DX12Device.GetDX12CBVSRVHeap();
	constexpr uint32_t numDescPerFrame = EngineConfig::ConstantBufferCount;
	constexpr uint32_t numDescPerThread = 0;
	constexpr uint32_t maxWorkers = 0;

	const uint32_t frameIndex = currBackBufferIndex;
	const uint32_t threadIndex = 0;

	uint32_t cbvSliceIndex = cbvSrvHeap->CalcHeapSliceShareBlock(frameIndex, threadIndex, numDescPerFrame, numDescPerThread, maxWorkers);
	HeapSlice cbvSlice = cbvSrvHeap->Offset(cbvSliceIndex);
	HeapSlice texSlice = cbvSrvHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount);
	HeapSlice matSlice = cbvSrvHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + 2 * EngineConfig::MaxTextureCount);
	HeapSlice shadowMapSlice = cbvSrvHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + 2 * EngineConfig::MaxTextureCount + 2);
	///////////////shadow test block
	CD3DX12_CPU_DESCRIPTOR_HANDLE shadowDSVOffsetHandle = static_cast<CD3DX12_CPU_DESCRIPTOR_HANDLE>(m_DX12Device.GetOffsetCPUHandle(
		m_DX12Device.GetDX12DSVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
		2 * m_DX12Device.GetDX12SwapChain()->GetSwapChainBufferCount(), // maind render + msaa render,
		m_DX12Device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)));
	m_DX12FrameBuffer.BeginShadowRender(m_DX12Device.GetDX12CommandList(), m_DX12Device.GetDX12ShadowManager(), shadowDSVOffsetHandle);
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetPipelineState(m_DX12Device.GetDX12PSO()->GetPipelineState(1));
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootDescriptorTable(0, cbvSlice.gpuDescHandle);
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootDescriptorTable(1, texSlice.gpuDescHandle);
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootDescriptorTable(2, matSlice.gpuDescHandle);

	for (uint32_t i = 0; i < m_DX12Device.GetRenderItemSize(); ++i)
	{
		UINT mat = m_DX12Device.GetDX12RenderItem(i).GetMaterialIndex();
		UINT tex = m_DX12Device.GetDX12RenderItem(i).GetTextureIndex();
		UINT obj = m_DX12Device.GetDX12RenderItem(i).GetObjectConstantIndex();
		struct RootPush { UINT m, t, o; } push{ mat, tex, obj };
		m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRoot32BitConstants(4, 3, &push, 0);

		m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetPrimitiveTopology(m_DX12Device.GetDX12RenderItem(i).GetRenderGeometry()->GetPrimitiveTopologyType());
		m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetVertexBuffers(0, 1, m_DX12Device.GetDX12RenderItem(i).GetRenderGeometry()->GetDX12VertexBufferView()->GetVertexBufferView());
		m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetIndexBuffer(m_DX12Device.GetDX12RenderItem(i).GetRenderGeometry()->GetDX12IndexBufferView()->GetIndexBufferView());
		m_DX12Device.GetDX12CommandList()->GetCommandList()->DrawIndexedInstanced(
			m_DX12Device.GetDX12RenderItem(i).GetRenderGeometry()->GetIndexCount(),
			1,
			m_DX12Device.GetDX12RenderItem(i).GetStartIndexLocation(),
			m_DX12Device.GetDX12RenderItem(i).GetBaseVertexLocation(),
			0);
	}
	m_DX12FrameBuffer.EndShadowRender(m_DX12Device.GetDX12CommandList(), m_DX12Device.GetDX12ShadowManager());
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootDescriptorTable(3, shadowMapSlice.gpuDescHandle);
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetPipelineState(m_DX12Device.GetDX12PSO()->GetPipelineState(0));
	//////////////


	CD3DX12_CPU_DESCRIPTOR_HANDLE tmpDSVOffsetHandle = static_cast<CD3DX12_CPU_DESCRIPTOR_HANDLE>(m_DX12Device.GetOffsetCPUHandle(
		m_DX12Device.GetDX12DSVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
		2 * m_DX12Device.GetDX12SwapChain()->GetSwapChainBufferCount(), // maind render + msaa render,
		m_DX12Device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)));

	m_DX12FrameBuffer.BeginMainPass(m_DX12Device.GetDX12CommandList(), currBackBufferIndex);
	PIXEndEvent(m_DX12Device.GetDX12CommandList()->GetCommandList()); //pix frame start setting marking end

	//m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootDescriptorTable(0, cbvSlice.gpuDescHandle);
	//m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootDescriptorTable(1, texSlice.gpuDescHandle);
	//m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRootDescriptorTable(2, matSlice.gpuDescHandle);

	PIXBeginEvent(m_DX12Device.GetDX12CommandList()->GetCommandList(), PIX_COLOR(0, 128, 255), L"MainDraw (%d items)", m_DX12Device.GetRenderItemSize()); //pix marking ~ main draw
	for (int renderItemIndex = 0; renderItemIndex < m_DX12Device.GetRenderItemSize(); renderItemIndex++)
	{
		UINT matIndex = m_DX12Device.GetDX12RenderItem(renderItemIndex).GetMaterialIndex();
		UINT texIndex = m_DX12Device.GetDX12RenderItem(renderItemIndex).GetTextureIndex();
		UINT objIndex = m_DX12Device.GetDX12RenderItem(renderItemIndex).GetObjectConstantIndex();
		struct RootPush { UINT matIdx; UINT texIdx; UINT worldIdx; } push{ matIndex, texIndex, objIndex };
		m_DX12Device.GetDX12CommandList()->GetCommandList()->SetGraphicsRoot32BitConstants(4, 3, &push, 0);

		m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetPrimitiveTopology(m_DX12Device.GetDX12RenderItem(renderItemIndex).GetRenderGeometry()->GetPrimitiveTopologyType());
		m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetVertexBuffers(0, 1, m_DX12Device.GetDX12RenderItem(renderItemIndex).GetRenderGeometry()->GetDX12VertexBufferView()->GetVertexBufferView());
		m_DX12Device.GetDX12CommandList()->GetCommandList()->IASetIndexBuffer(m_DX12Device.GetDX12RenderItem(renderItemIndex).GetRenderGeometry()->GetDX12IndexBufferView()->GetIndexBufferView());
		m_DX12Device.GetDX12CommandList()->GetCommandList()->DrawIndexedInstanced(
			m_DX12Device.GetDX12RenderItem(renderItemIndex).GetRenderGeometry()->GetIndexCount(),
			1,
			m_DX12Device.GetDX12RenderItem(renderItemIndex).GetStartIndexLocation(),
			m_DX12Device.GetDX12RenderItem(renderItemIndex).GetBaseVertexLocation(),
			0);
	}
	PIXEndEvent(m_DX12Device.GetDX12CommandList()->GetCommandList()); //pix main draw marking end

	PIXBeginEvent(m_DX12Device.GetDX12CommandList()->GetCommandList(), PIX_COLOR(255, 128, 0), L"EndFrame/Resolve"); //pix marking ~ resorve RTV
	m_DX12FrameBuffer.EndMainPass(m_DX12Device.GetDX12CommandList(), currBackBufferIndex, m_DX12Device.GetDX12SwapChain()->GetRenderTargetFormat(), m_DX12Device.GetDX12ShadowManager());
	PIXEndEvent(m_DX12Device.GetDX12CommandList()->GetCommandList());
	//////////////////////////////////imgui drawing
	// imgui render block
	PIXBeginEvent(m_DX12Device.GetDX12CommandList()->GetCommandList(), PIX_COLOR(255, 0, 255), L"ImGui"); //pix marking ~ imgui rendering
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
	ID3D12DescriptorHeap* SRVheaps[] = { m_DX12Device.GetDX12ImGuiHeap()->GetDescHeap() };
	m_DX12Device.GetDX12CommandList()->GetCommandList()->SetDescriptorHeaps(1, SRVheaps);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_DX12Device.GetDX12CommandList()->GetCommandList());
	PIXEndEvent(m_DX12Device.GetDX12CommandList()->GetCommandList()); //pix imgui marking end
	m_DX12FrameBuffer.SetBackBufferPresent(m_DX12Device.GetDX12CommandList(), currBackBufferIndex);
	m_timer.EndGPU(m_DX12Device.GetDX12CommandList()->GetCommandList(), currBackBufferIndex); // << GPU TIMER END
	//////////////////////////////////////////////////////////////////////////////////

	m_DX12Device.GetDX12CommandList()->GetCommandList()->Close();
	ID3D12CommandList* lists[] = { m_DX12Device.GetDX12CommandList()->GetCommandList() };
	m_DX12Device.GetDX12CommandList()->GetCommandQueue()->ExecuteCommandLists(1, lists);

	const uint64_t fenceValue = m_DX12Device.GetDX12CommandList()->Signal();
	m_DX12Device.GetFrameResource(currBackBufferIndex)->SetFenceValue(fenceValue);
	m_timer.EndCPU(currBackBufferIndex); // << CPU TIMER END
	m_DX12FrameBuffer.Present(&m_DX12Device);

	PIXEndEvent(m_DX12Device.GetDX12CommandList()->GetCommandQueue()); //pix frame marking end
	sFrameId++;
}

void RenderDX12::RecordAndSubmit_Multi()
{
	/*
	// current version of multi-thread method was implemented only on drawing commands
	*/
	PIXBeginEvent(m_DX12Device.GetDX12CommandList()->GetCommandQueue(), PIX_COLOR(0, 255, 0), L"Frame %u", sFrameId); //pix marking ~ total frame

	m_DX12Device.SetCurrentBackBufferIndex(m_DX12Device.GetDX12SwapChain()->GetSwapChain()->GetCurrentBackBufferIndex());
	const uint32_t currBackBufferIndex = m_DX12Device.GetDX12SwapChain()->GetSwapChain()->GetCurrentBackBufferIndex();

	m_DX12FrameBuffer.CheckFence(&m_DX12Device, currBackBufferIndex); // << GPU WAIT
	////////////////imgui timer set
	float gpuMS = m_timer.GetElapsedGPUMS(currBackBufferIndex);
	float cpuMS = m_timer.GetElapsedCPUMS(currBackBufferIndex);
	m_DX12Device.GetFrameResource(currBackBufferIndex)->ResetAllAllocators();
	m_DX12Device.GetFrameResource(currBackBufferIndex)->EnsureWorkerCapacity(m_DX12Device.GetDevice(), EngineConfig::NumThreadWorker);
	m_DX12Device.GetDX12CommandList()->ResetList(m_DX12Device.GetDX12PSO()->GetPipelineState(), m_DX12Device.GetFrameResource(currBackBufferIndex)->GetCommandAllocator()); //main thread reset

	m_timer.BeginCPU(currBackBufferIndex); // << imgui CPU TIMER START

	PIXBeginEvent(m_DX12Device.GetDX12CommandList()->GetCommandList(), PIX_COLOR(0, 180, 255), L"BeginFrame"); // pix marking ~ frame start setting

	m_timer.BeginGPU(m_DX12Device.GetDX12CommandList()->GetCommandList(), currBackBufferIndex); // << imgui GPU TIMER START

	m_DX12Device.UpdateFrameResource();

	CD3DX12_CPU_DESCRIPTOR_HANDLE tmpDSVOffsetHandle = static_cast<CD3DX12_CPU_DESCRIPTOR_HANDLE>(m_DX12Device.GetOffsetCPUHandle(
		m_DX12Device.GetDX12DSVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
		2 * m_DX12Device.GetDX12SwapChain()->GetSwapChainBufferCount(), // maind render + msaa render,
		m_DX12Device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)));

	m_DX12FrameBuffer.BeginMainPass(m_DX12Device.GetDX12CommandList(), currBackBufferIndex);
	PIXEndEvent(m_DX12Device.GetDX12CommandList()->GetCommandList()); //pix frame start setting marking end
	m_DX12Device.GetDX12CommandList()->GetCommandList()->Close();

	auto* cbvSrvHeap = m_DX12Device.GetDX12CBVSRVHeap();
	constexpr uint32_t numDescPerFrame = EngineConfig::ConstantBufferCount;
	constexpr uint32_t numDescPerThread = 0;
	constexpr uint32_t maxWorkers = 0;
	const uint32_t numWorkers = m_DX12Device.GetFrameResource(currBackBufferIndex)->GetWorkerAllocatorCount();
	const uint32_t frameIndex = currBackBufferIndex;
	const uint32_t threadIndex = 0;
	const uint32_t numItems = m_DX12Device.GetRenderItemSize();
	const uint32_t chunkSize = (numItems + numWorkers - 1) / numWorkers;

	//if empty, create worker thread which do RenderDX12::AllocateWorkerDrawingCommand
	if (m_workerThreads.empty())
	{
		m_workerJobDrawing.resize(numWorkers);
		for (uint32_t i = 0; i < numWorkers; ++i)
			m_workerThreads.emplace_back(&RenderDX12::AllocateWorkerDrawingCommand, this, i);
	}

	uint32_t cbvSliceIndex = cbvSrvHeap->CalcHeapSliceShareBlock(frameIndex, threadIndex, numDescPerFrame, numDescPerThread, maxWorkers);
	HeapSlice cbvSlice = cbvSrvHeap->Offset(cbvSliceIndex);
	HeapSlice texSlice = cbvSrvHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount);
	HeapSlice matSlice = cbvSrvHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + 2 * EngineConfig::MaxTextureCount);

	{//mutex block
		std::unique_lock<std::mutex> lock(m_workerMutex);
		m_pendingWorkers = numWorkers;
		for (uint32_t i = 0; i < numWorkers; ++i)
		{
			uint32_t beginIndex = i * chunkSize;
			uint32_t endIndex = min(beginIndex + chunkSize, numItems);
			m_workerJobDrawing[i] = { beginIndex, endIndex, currBackBufferIndex, cbvSlice, texSlice, matSlice, true };
		}
	}
	m_workerCV.notify_all();//wake up workers (do drawing commands)

	m_DX12Device.GetPostDrawDX12CommandList()->ResetList(m_DX12Device.GetDX12PSO()->GetPipelineState(), m_DX12Device.GetFrameResource(currBackBufferIndex)->GetCommandAllocator());

	PIXBeginEvent(m_DX12Device.GetPostDrawDX12CommandList()->GetCommandList(), PIX_COLOR(255, 128, 0), L"EndFrame/Resolve"); //pix marking ~ resorve RTV
	m_DX12FrameBuffer.EndMainPass(m_DX12Device.GetPostDrawDX12CommandList(), currBackBufferIndex, m_DX12Device.GetDX12SwapChain()->GetRenderTargetFormat(), m_DX12Device.GetDX12ShadowManager());
	PIXEndEvent(m_DX12Device.GetPostDrawDX12CommandList()->GetCommandList());
	//////////////////////////////////imgui drawing
	// imgui render block
	PIXBeginEvent(m_DX12Device.GetPostDrawDX12CommandList()->GetCommandList(), PIX_COLOR(255, 0, 255), L"ImGui"); //pix marking ~ imgui rendering
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

	m_DX12Device.GetPostDrawDX12CommandList()->GetCommandList()->OMSetRenderTargets(1, &backbufRTV, FALSE, nullptr);
	ID3D12DescriptorHeap* SRVheaps[] = { m_DX12Device.GetDX12ImGuiHeap()->GetDescHeap() };
	m_DX12Device.GetPostDrawDX12CommandList()->GetCommandList()->SetDescriptorHeaps(1, SRVheaps);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_DX12Device.GetPostDrawDX12CommandList()->GetCommandList());
	PIXEndEvent(m_DX12Device.GetPostDrawDX12CommandList()->GetCommandList()); //pix imgui marking end
	m_DX12FrameBuffer.SetBackBufferPresent(m_DX12Device.GetPostDrawDX12CommandList(), currBackBufferIndex);
	m_timer.EndGPU(m_DX12Device.GetPostDrawDX12CommandList()->GetCommandList(), currBackBufferIndex); // << GPU TIMER END
	//////////////////////////////////////////////////////////////////////////////////
	std::unique_lock<std::mutex> lock(m_workerMutex);
	m_workerDoneCV.wait(lock, [&] { return m_pendingWorkers == 0; }); //wait until notify(workers do all of their works)

	std::vector<ID3D12CommandList*> submitCommandLists; //commands should be submitted as sequentially
	submitCommandLists.reserve(2 + numWorkers); //main + workers + finish
	submitCommandLists.push_back(m_DX12Device.GetDX12CommandList()->GetCommandList());
	for (int i = 0; i < m_DX12Device.GetWorkerDX12CommandListSize(); i++) submitCommandLists.push_back(m_DX12Device.GetWorkerDX12CommandList(i)->GetCommandList());
	submitCommandLists.push_back(m_DX12Device.GetPostDrawDX12CommandList()->GetCommandList());

	m_DX12Device.GetPostDrawDX12CommandList()->GetCommandList()->Close();
	m_DX12Device.GetDX12CommandList()->GetCommandQueue()->ExecuteCommandLists((UINT)submitCommandLists.size(), submitCommandLists.data());

	const uint64_t fenceValue = m_DX12Device.GetDX12CommandList()->Signal();
	m_DX12Device.GetFrameResource(currBackBufferIndex)->SetFenceValue(fenceValue);
	m_timer.EndCPU(currBackBufferIndex); // << CPU TIMER END
	m_DX12FrameBuffer.Present(&m_DX12Device);

	PIXEndEvent(m_DX12Device.GetDX12CommandList()->GetCommandQueue()); //pix frame marking end
	sFrameId++;
}

void RenderDX12::ShutDown()
{
	// Cleanup
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		m_terminateWorkers = true;
	}
	m_workerCV.notify_all();
	for (auto& t : m_workerThreads)
	{
		if (t.joinable()) t.join();
	}
	m_DX12Device.GetDX12CommandList()->FlushCommandQueue();
}