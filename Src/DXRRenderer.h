#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <vector>

//#define PIX_PROFILE

#ifdef PIX_PROFILE
#include <DXProgrammableCapture.h>
#endif

using namespace Microsoft::WRL;

#define NUM_BACK_BUFFER 3

static const D3D12_HEAP_PROPERTIES UploadHeapProps =
{
	D3D12_HEAP_TYPE_UPLOAD,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0,
	0,
};

static const D3D12_HEAP_PROPERTIES DefaultHeapProps =
{
	D3D12_HEAP_TYPE_DEFAULT,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0,
	0,
};

struct AccelerationStructureBuffers
{
	ComPtr<ID3D12Resource> Scratch;
	ComPtr<ID3D12Resource> Result;
	ComPtr<ID3D12Resource> InstanceDesc;
};

struct RayGenRootSigDesc
{
	D3D12_ROOT_SIGNATURE_DESC Desc = {};
	std::vector<D3D12_DESCRIPTOR_RANGE> Ranges;
	std::vector<D3D12_ROOT_PARAMETER> RootParams;
};

class DXRRenderer
{
public:
	void Init(HWND WinHandle, int BackbufferW, int BackbufferH);

	void OnFrameBegin();
	void Draw();
	void OnFrameEnd();

	void Exit();

	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(unsigned int Count, D3D12_DESCRIPTOR_HEAP_TYPE Type, bool bShaderVisible = false);

	D3D12_CPU_DESCRIPTOR_HANDLE CreateRTV(ID3D12Resource* Resource, ID3D12DescriptorHeap* Heap, unsigned int& UsedHeapEntries, DXGI_FORMAT Format);

	ComPtr<ID3D12Resource> CreateBuffer(UINT64 Size, D3D12_RESOURCE_FLAGS Flags, D3D12_RESOURCE_STATES State, const D3D12_HEAP_PROPERTIES& HeapProps);
	AccelerationStructureBuffers CreateBottomLevelAS(ID3D12GraphicsCommandList4* CmdList, ID3D12Resource* Resource);
	AccelerationStructureBuffers CreateTopLevelAS(ID3D12GraphicsCommandList4* CmdList, ID3D12Resource* BottomLevelAS, UINT64& TlasSize);
	void CreateAccelerationStructures();
	ComPtr<ID3D12Resource> CreateTriangleVBTest();
	ComPtr<ID3D12Resource> CreateSphereVB();

	void CreateAABBAttributeBuffer();

	RayGenRootSigDesc CreateRayGenRootDesc();
	ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& Desc);

	void ResourceBarrier(ID3D12GraphicsCommandList4* CmdList, ID3D12Resource* Resource, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter);
	UINT64 SubmitCommandList(ID3D12GraphicsCommandList4* CmdList, ID3D12CommandQueue* pCmdQueue, ID3D12Fence* Fence, UINT64 FenceValue);

	void CreateRayTracingPipelineState();
	void CreateShaderTable();
	void CreateShaderResource();

private:
	ComPtr<ID3D12Device5> Device;
	ComPtr<ID3D12CommandQueue> CommandQueue;
	ComPtr<IDXGISwapChain3> SwapChain;
	ComPtr<ID3D12GraphicsCommandList4> CommandList;

	HWND CurrentWindowHandle;
	int BackbufferWidth;
	int BackbufferHeight;

#ifdef PIX_PROFILE 
	ComPtr<IDXGraphicsAnalysis> DXGA;
#endif

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
	unsigned int CurrentBackbufferIdx = 0;

	ComPtr<ID3D12Resource> VertexBuffer;
	ComPtr<ID3D12Resource> TopLevelAS;
	ComPtr<ID3D12Resource> BottomLevelAS;
	UINT64 TlasSize = 0;

	ComPtr<ID3D12StateObject> PipelineStateObject;
	ComPtr<ID3D12RootSignature> GlobalRootSig;

	ComPtr<ID3D12Resource> ShaderTable;
	UINT32 ShaderTableEntrySize = 0;

	ComPtr<ID3D12Resource> OutputResource;
	ComPtr<ID3D12Resource> AabbAttributes;
	ComPtr<ID3D12DescriptorHeap> SrvUavHeap;
	static const UINT32 SrvUavHeapSize = 2;
};