#include "DXRRenderer.h"

//#define DUMP_MSG(msg, hr) { char hr_msg[512]; FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, hr_msg, ARRAYSIZE(hr_msg), nullptr); OutputDebugString(hr_msg)}

#define CHECK_HR(Rslt) {HRESULT hr = Rslt; if(FAILED(hr)){ exit(1);}}

void DXRRenderer::Init(HWND WinHandle, int BackbufferW, int BackbufferH)
{
	CurrentWindowHandle = WinHandle;
	BackbufferWidth = BackbufferW;
	BackbufferHeight = BackbufferH;
	FenceValue = 0;
#ifdef _DEBUG
	ComPtr<ID3D12Debug> Debug;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&Debug))))
	{
		Debug->EnableDebugLayer();
	}
#endif
	
	ComPtr<IDXGIFactory4> Factory4;
	CHECK_HR(CreateDXGIFactory1(IID_PPV_ARGS(&Factory4)));
	ComPtr<IDXGIAdapter1> Adapter1;
	for (int i = 0; DXGI_ERROR_NOT_FOUND != Factory4->EnumAdapters1(i, &Adapter1); i++)
	{
		DXGI_ADAPTER_DESC1 Desc;
		Adapter1->GetDesc1(&Desc);

		if (Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

		D3D12CreateDevice(Adapter1.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&Device));
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 Feature5;
		CHECK_HR(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &Feature5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)));
		if (Feature5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
			exit(1);
		else
			break;
	}

	D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
	CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	CHECK_HR(Device->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(&CommandQueue)));

	DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
	SwapChainDesc.BufferCount = NUM_BACK_BUFFER; //Triple-buffer
	SwapChainDesc.Width = BackbufferWidth;
	SwapChainDesc.Height = BackbufferHeight;
	SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	SwapChainDesc.SampleDesc.Count = 1;

	//CreateSwapChainForHwnd not support IDXGISwapChain3 directly
	ComPtr<IDXGISwapChain1> SwapChain1;
	CHECK_HR(Factory4->CreateSwapChainForHwnd(CommandQueue.Get(), WinHandle, &SwapChainDesc, nullptr, nullptr, &SwapChain1));
	CHECK_HR(SwapChain1->QueryInterface(IID_PPV_ARGS(&SwapChain)));

	DescHeap = CreateDescriptorHeap(NUM_BACK_BUFFER, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (unsigned int i = 0; i < NUM_BACK_BUFFER; i++)
	{
		Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&FrameObjects[i].CmdAllocator));
		SwapChain->GetBuffer(i, IID_PPV_ARGS(&FrameObjects[i].SwapChainBuffer));
		FrameObjects[i].RtvHandle = CreateRTV(FrameObjects[i].SwapChainBuffer.Get(), DescHeap.Get(), i, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	}

	Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, FrameObjects[0].CmdAllocator.Get(), nullptr, IID_PPV_ARGS(&CommandList));
	Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&RenderFence));
	FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

}

ComPtr<ID3D12DescriptorHeap> DXRRenderer::CreateDescriptorHeap(unsigned int Count, D3D12_DESCRIPTOR_HEAP_TYPE Type, bool bShaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
	Desc.NumDescriptors = Count;
	Desc.Type = Type;
	Desc.Flags = bShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ComPtr<ID3D12DescriptorHeap> Heap;
	Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&Heap));
	return Heap;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRRenderer::CreateRTV(ID3D12Resource* Resource, ID3D12DescriptorHeap* Heap, unsigned int& UsedHeapEntries, DXGI_FORMAT Format)
{
	D3D12_RENDER_TARGET_VIEW_DESC Desc = {};
	Desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	Desc.Format = Format;
	Desc.Texture2D.MipSlice = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = Heap->GetCPUDescriptorHandleForHeapStart();
	RTVHandle.ptr += UsedHeapEntries * Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//UsedHeapEntries++;
	Device->CreateRenderTargetView(Resource, &Desc, RTVHandle);
	return RTVHandle;
}

void DXRRenderer::OnFrameBegin()
{
	CurrentBackbufferIdx = SwapChain->GetCurrentBackBufferIndex();
}

void DXRRenderer::OnFrameEnd()
{
	ResourceBarrier(CommandList.Get(), FrameObjects[CurrentBackbufferIdx].SwapChainBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	FenceValue = SubmitCommandList(CommandList.Get(), CommandQueue.Get(), RenderFence.Get(), FenceValue);
	SwapChain->Present(0, 0);

	// Prepare the command list for the next frame
	unsigned int bufferIndex = SwapChain->GetCurrentBackBufferIndex();

	// Make sure we have the new back-buffer is ready
	if (FenceValue > NUM_BACK_BUFFER)
	{
		RenderFence->SetEventOnCompletion(FenceValue - NUM_BACK_BUFFER + 1, FenceEvent);
		WaitForSingleObject(FenceEvent, INFINITE);
	}

	FrameObjects[bufferIndex].CmdAllocator->Reset();
	CommandList->Reset(FrameObjects[bufferIndex].CmdAllocator.Get(), nullptr);
}

void DXRRenderer::Draw()
{
	OnFrameBegin();

	unsigned int NowIdx = CurrentBackbufferIdx;
	const float clearColor[4] = { 0.4f, 0.6f, 0.2f, 1.0f };
	ResourceBarrier(CommandList.Get(), FrameObjects[NowIdx].SwapChainBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	CommandList->ClearRenderTargetView(FrameObjects[NowIdx].RtvHandle, clearColor, 0, nullptr);

	OnFrameEnd();
}

void DXRRenderer::ResourceBarrier(ID3D12GraphicsCommandList4* CmdList, ID3D12Resource* Resource, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter)
{
	D3D12_RESOURCE_BARRIER Barrier = {};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = Resource;
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	Barrier.Transition.StateBefore = StateBefore;
	Barrier.Transition.StateAfter = StateAfter;
	CmdList->ResourceBarrier(1, &Barrier);
}

UINT64 DXRRenderer::SubmitCommandList(ID3D12GraphicsCommandList* CmdList, ID3D12CommandQueue* CmdQueue, ID3D12Fence* Fence, UINT64 FenceValue)
{
	CmdList->Close();
	ID3D12CommandList* pGraphicsList = CmdList;
	CmdQueue->ExecuteCommandLists(1, &pGraphicsList);
	FenceValue++;
	CmdQueue->Signal(Fence, FenceValue);
	return FenceValue;
}