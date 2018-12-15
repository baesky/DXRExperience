#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <dxgi1_4.h>

using namespace Microsoft::WRL;

#define NUM_BACK_BUFFER 3

class DXRRenderer
{
public:
	void Init(HWND WinHandle, int BackbufferW, int BackbufferH);

	void OnFrameBegin();
	void Draw();
	void OnFrameEnd();

	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(unsigned int Count, D3D12_DESCRIPTOR_HEAP_TYPE Type, bool bShaderVisible = false);

	D3D12_CPU_DESCRIPTOR_HANDLE CreateRTV(ID3D12Resource* Resource, ID3D12DescriptorHeap* Heap, unsigned int& UsedHeapEntries, DXGI_FORMAT Format);

	void ResourceBarrier(ID3D12GraphicsCommandList4* CmdList, ID3D12Resource* Resource, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter);
	UINT64 SubmitCommandList(ID3D12GraphicsCommandList* CmdList, ID3D12CommandQueue* pCmdQueue, ID3D12Fence* Fence, UINT64 FenceValue);

private:
	ComPtr<ID3D12Device5> Device;
	ComPtr<ID3D12CommandQueue> CommandQueue;
	ComPtr<IDXGISwapChain3> SwapChain;
	ComPtr<ID3D12GraphicsCommandList4> CommandList;

	HWND CurrentWindowHandle;
	int BackbufferWidth;
	int BackbufferHeight;

	//should kick out from here
	ComPtr<ID3D12DescriptorHeap> DescHeap;
	struct
	{
		ComPtr<ID3D12CommandAllocator> CmdAllocator;
		ComPtr<ID3D12Resource> SwapChainBuffer;
		D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle;
	} FrameObjects[NUM_BACK_BUFFER];
	ComPtr<ID3D12Fence> RenderFence;
	HANDLE FenceEvent;
	UINT64 FenceValue;
	unsigned int CurrentBackbufferIdx;

};