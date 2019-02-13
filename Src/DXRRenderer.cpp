#include "DXRRenderer.h"
#include <array>
#include <DirectXMath.h>
#include "DXILLib.h"

using namespace DirectX;
using namespace std;


#define CHECK_HR(Rslt) {HRESULT hr = Rslt; if(FAILED(hr)){ exit(1);}}
#define ALIGN_TO(Alignment, Val) (((Val + Alignment - 1) / Alignment) * Alignment)

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
	
#ifdef PIX_PROFILE 
	HRESULT hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&DXGA));


	DXGA->BeginCapture();


#endif

	ComPtr<IDXGIFactory4> Factory4;
	CHECK_HR(CreateDXGIFactory1(IID_PPV_ARGS(&Factory4)));
	ComPtr<IDXGIAdapter1> Adapter1;
	for (int i = 0; DXGI_ERROR_NOT_FOUND != Factory4->EnumAdapters1(i, &Adapter1); i++)
	{
		DXGI_ADAPTER_DESC1 Desc;
		Adapter1->GetDesc1(&Desc);

		if (Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

		CHECK_HR(D3D12CreateDevice(Adapter1.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&Device)));
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
	CHECK_HR(SwapChain1->QueryInterface(IID_PPV_ARGS(SwapChain.GetAddressOf())));

	DescHeap = CreateDescriptorHeap(NUM_BACK_BUFFER, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (unsigned int i = 0; i < NUM_BACK_BUFFER; i++)
	{
		D3D_CALL(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(FrameObjects[i].CmdAllocator.GetAddressOf())));
		D3D_CALL(SwapChain->GetBuffer(i, IID_PPV_ARGS(FrameObjects[i].SwapChainBuffer.GetAddressOf())));
		FrameObjects[i].RtvHandle = CreateRTV(FrameObjects[i].SwapChainBuffer.Get(), DescHeap.Get(), i, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	}

	D3D_CALL(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, FrameObjects[0].CmdAllocator.Get(), nullptr, IID_PPV_ARGS(CommandList.GetAddressOf())));
	D3D_CALL(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(RenderFence.GetAddressOf())));
	FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	//
	CreateAccelerationStructures();

	CreateRayTracingPipelineState();

	CreateShaderResource();

	CreateShaderTable();
}

ComPtr<ID3D12DescriptorHeap> DXRRenderer::CreateDescriptorHeap(unsigned int Count, D3D12_DESCRIPTOR_HEAP_TYPE Type, bool bShaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
	Desc.NumDescriptors = Count;
	Desc.Type = Type;
	Desc.Flags = bShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ComPtr<ID3D12DescriptorHeap> Heap;
	D3D_CALL(Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&Heap)));
	return Heap;
}

ComPtr<ID3D12Resource> DXRRenderer::CreateBuffer(UINT64 Size, D3D12_RESOURCE_FLAGS Flags, D3D12_RESOURCE_STATES State, const D3D12_HEAP_PROPERTIES& HeapProps)
{
	D3D12_RESOURCE_DESC BuffDesc = {};
	BuffDesc.Alignment = 0;
	BuffDesc.DepthOrArraySize = 1;
	BuffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	BuffDesc.Flags = Flags;
	BuffDesc.Format = DXGI_FORMAT_UNKNOWN;
	BuffDesc.Height = 1;
	BuffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	BuffDesc.MipLevels = 1;
	BuffDesc.SampleDesc.Count = 1;
	BuffDesc.SampleDesc.Quality = 0;
	BuffDesc.Width = Size;

	ComPtr<ID3D12Resource> Buffer;
	D3D_CALL(Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &BuffDesc, State, nullptr, IID_PPV_ARGS(&Buffer)));
	return Buffer;
}

AccelerationStructureBuffers DXRRenderer::CreateBottomLevelAS(ID3D12GraphicsCommandList4* CmdList, ID3D12Resource* Resource)
{
	D3D12_RAYTRACING_GEOMETRY_DESC GeomDesc = {};
	//GeomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	//GeomDesc.Triangles.VertexBuffer.StartAddress = Resource->GetGPUVirtualAddress();
	//GeomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(XMFLOAT3);
	//GeomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	//GeomDesc.Triangles.VertexCount = 3;
	//GeomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	GeomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
	GeomDesc.AABBs.AABBCount = 1;
	GeomDesc.AABBs.AABBs.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB);
	GeomDesc.AABBs.AABBs.StartAddress = Resource->GetGPUVirtualAddress();
	GeomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Inputs = {};
	Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	Inputs.NumDescs = 1;
	Inputs.pGeometryDescs = &GeomDesc;
	Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO Info = {};
	Device->GetRaytracingAccelerationStructurePrebuildInfo(&Inputs, &Info);

	AccelerationStructureBuffers Buffers;
	Buffers.Scratch = CreateBuffer(Info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, DefaultHeapProps);
	Buffers.Result = CreateBuffer(Info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, DefaultHeapProps);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC ASDesc = {};
	ASDesc.Inputs = Inputs;
	ASDesc.DestAccelerationStructureData = Buffers.Result->GetGPUVirtualAddress();
	ASDesc.ScratchAccelerationStructureData = Buffers.Scratch->GetGPUVirtualAddress();

	CmdList->BuildRaytracingAccelerationStructure(&ASDesc, 0, nullptr);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	D3D12_RESOURCE_BARRIER UAVBarrier = {};
	UAVBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	UAVBarrier.UAV.pResource = Buffers.Result.Get();
	CmdList->ResourceBarrier(1, &UAVBarrier);

	return Buffers;

}

AccelerationStructureBuffers DXRRenderer::CreateTopLevelAS(ID3D12GraphicsCommandList4* CmdList, ID3D12Resource* BottomLevelAS, UINT64& TlasSize)
{
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Inputs = {};
	Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	Inputs.NumDescs = 1;
	Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO Info;
	Device->GetRaytracingAccelerationStructurePrebuildInfo(&Inputs, &Info);

	AccelerationStructureBuffers Buffers;
	Buffers.Scratch = CreateBuffer(Info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, DefaultHeapProps);
	Buffers.Result = CreateBuffer(Info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, DefaultHeapProps);
	TlasSize = Info.ResultDataMaxSizeInBytes;

	Buffers.InstanceDesc = CreateBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, UploadHeapProps);
	D3D12_RAYTRACING_INSTANCE_DESC* InstanceDesc;
	Buffers.InstanceDesc->Map(0, nullptr, (void**)&InstanceDesc);
	
	InstanceDesc->InstanceID = 0;
	InstanceDesc->InstanceContributionToHitGroupIndex = 0;
	InstanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	XMFLOAT4X4 Mat{ 1.0f,.0f,.0f,.0f,
					.0f, 1.0f,.0f,.0f,
					.0f,.0f,1.0f,.0f,
					.0f,.0f,.0f,1.0f };
	memcpy(InstanceDesc->Transform, &Mat, sizeof(InstanceDesc->Transform));
	InstanceDesc->AccelerationStructure = BottomLevelAS->GetGPUVirtualAddress();
	InstanceDesc->InstanceMask = 0xFF;

	Buffers.InstanceDesc->Unmap(0, nullptr);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC ASDesc = {};
	ASDesc.Inputs = Inputs;
	ASDesc.Inputs.InstanceDescs = Buffers.InstanceDesc->GetGPUVirtualAddress();
	ASDesc.DestAccelerationStructureData = Buffers.Result->GetGPUVirtualAddress();
	ASDesc.ScratchAccelerationStructureData = Buffers.Scratch->GetGPUVirtualAddress();

	CmdList->BuildRaytracingAccelerationStructure(&ASDesc, 0, nullptr);

	D3D12_RESOURCE_BARRIER UAVBarrier = {};
	UAVBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	UAVBarrier.UAV.pResource = Buffers.Result.Get();
	CmdList->ResourceBarrier(1, &UAVBarrier);

	return Buffers;
}

ComPtr<ID3D12Resource> DXRRenderer::CreateSphereVB()
{
	XMINT3 AABBGrid = XMINT3(4, 1, 4);
	const XMFLOAT3 BasePosition = 
	{
		-(AABBGrid.x + AABBGrid.x - 1) / 2.0f,
		-(AABBGrid.y + AABBGrid.y - 1) / 2.0f,
		-(AABBGrid.z + AABBGrid.z - 1) / 2.0f,
	};

	XMFLOAT3 Stride = XMFLOAT3(2, 2, 2);
	auto InitializeAABB = [&](auto const& OffsetIdx, auto const& Size)
	{
		return D3D12_RAYTRACING_AABB{
			BasePosition.x + OffsetIdx.x * Stride.x,
			BasePosition.y + OffsetIdx.y * Stride.y,
			BasePosition.z + OffsetIdx.z * Stride.z,
			BasePosition.x + OffsetIdx.x * Stride.x + Size.x,
			BasePosition.y + OffsetIdx.y * Stride.y + Size.y,
			BasePosition.z + OffsetIdx.z * Stride.z + Size.z,
		};
	};

	D3D12_RAYTRACING_AABB Sphere = InitializeAABB(XMFLOAT3(2.25f, 0, 1.075f), XMFLOAT3(1, 1, 1));
	ComPtr<ID3D12Resource> Buffer = CreateBuffer(sizeof(Sphere), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, UploadHeapProps);
	UINT8* DstData;
	Buffer->Map(0, nullptr, (void**)&DstData);
	memcpy(DstData, &Sphere, sizeof(Sphere));
	Buffer->Unmap(0, nullptr);

	return Buffer;
}

ComPtr<ID3D12Resource> DXRRenderer::CreateTriangleVBTest()
{
	const XMFLOAT3 Verts[] = 
	{
		XMFLOAT3(0, 1, 0),
		XMFLOAT3(0.866f, -0.5f, 0),
		XMFLOAT3(-0.866f, -0.5f, 0),
	};

	ComPtr<ID3D12Resource> Buffer = CreateBuffer(sizeof(Verts), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, UploadHeapProps);
	UINT8* DstData;
	Buffer->Map(0, nullptr, (void**)&DstData);
	memcpy(DstData, Verts, sizeof(Verts));
	Buffer->Unmap(0, nullptr);

	return Buffer;
}

void DXRRenderer::CreateAccelerationStructures()
{
	VertexBuffer = CreateSphereVB();//CreateTriangleVBTest();
	AccelerationStructureBuffers BLBuffer = CreateBottomLevelAS(CommandList.Get(), VertexBuffer.Get());
	AccelerationStructureBuffers TLBuffer = CreateTopLevelAS(CommandList.Get(), BLBuffer.Result.Get(), TlasSize);

	FenceValue = SubmitCommandList(CommandList.Get(), CommandQueue.Get(), RenderFence.Get(), FenceValue);
	RenderFence->SetEventOnCompletion(FenceValue, FenceEvent);
	WaitForSingleObject(FenceEvent, INFINITE);
	UINT32 BufferIdx = SwapChain->GetCurrentBackBufferIndex();
	CommandList->Reset(FrameObjects[0].CmdAllocator.Get(), nullptr);

	TopLevelAS = TLBuffer.Result;
	BottomLevelAS = BLBuffer.Result;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRRenderer::CreateRTV(ID3D12Resource* Resource, ID3D12DescriptorHeap* Heap, unsigned int& UsedHeapEntries, DXGI_FORMAT Format)
{
	D3D12_RENDER_TARGET_VIEW_DESC Desc = {};
	Desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	Desc.Format = Format;
	Desc.Texture2D.MipSlice = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = Heap->GetCPUDescriptorHandleForHeapStart();
	RTVHandle.ptr += UsedHeapEntries * Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	Device->CreateRenderTargetView(Resource, &Desc, RTVHandle);
	return RTVHandle;
}

void DXRRenderer::OnFrameBegin()
{
	ID3D12DescriptorHeap* Heaps[] = { SrvUavHeap.Get() };
	CommandList->SetDescriptorHeaps(1, Heaps);
	CurrentBackbufferIdx = SwapChain->GetCurrentBackBufferIndex();
}

void DXRRenderer::OnFrameEnd()
{
	ResourceBarrier(CommandList.Get(), FrameObjects[CurrentBackbufferIdx].SwapChainBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
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

	ResourceBarrier(CommandList.Get(), OutputResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12_DISPATCH_RAYS_DESC RayTraceDesc = {};
	RayTraceDesc.Width = BackbufferWidth;
	RayTraceDesc.Height = BackbufferHeight;
	RayTraceDesc.Depth = 1;

	RayTraceDesc.RayGenerationShaderRecord.StartAddress = ShaderTable->GetGPUVirtualAddress();
	RayTraceDesc.RayGenerationShaderRecord.SizeInBytes = ShaderTableEntrySize;

	RayTraceDesc.MissShaderTable.StartAddress = ShaderTable->GetGPUVirtualAddress() + ShaderTableEntrySize;
	RayTraceDesc.MissShaderTable.StrideInBytes = ShaderTableEntrySize;
	RayTraceDesc.MissShaderTable.SizeInBytes = ShaderTableEntrySize;

	RayTraceDesc.HitGroupTable.StartAddress = ShaderTable->GetGPUVirtualAddress() + ShaderTableEntrySize * 2;
	RayTraceDesc.HitGroupTable.StrideInBytes = ShaderTableEntrySize;
	RayTraceDesc.HitGroupTable.SizeInBytes = ShaderTableEntrySize;

	CommandList->SetComputeRootSignature(EmptyRootSig.Get());
	CommandList->SetPipelineState1(PipelineStateObject.Get());
	CommandList->DispatchRays(&RayTraceDesc);

	ResourceBarrier(CommandList.Get(), OutputResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	ResourceBarrier(CommandList.Get(), FrameObjects[CurrentBackbufferIdx].SwapChainBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	CommandList->CopyResource(FrameObjects[CurrentBackbufferIdx].SwapChainBuffer.Get(), OutputResource.Get());

	OnFrameEnd();

#ifdef PIX_PROFILE 
	DXGA->EndCapture();
#endif
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

UINT64 DXRRenderer::SubmitCommandList(ID3D12GraphicsCommandList4* CmdList, ID3D12CommandQueue* CmdQueue, ID3D12Fence* Fence, UINT64 FenceValue)
{
	CmdList->Close();
	ID3D12CommandList* pGraphicsList = CmdList;
	CmdQueue->ExecuteCommandLists(1, &pGraphicsList);
	FenceValue++;
	CmdQueue->Signal(Fence, FenceValue);
	return FenceValue;
}

ComPtr<ID3D12RootSignature> DXRRenderer::CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& Desc)
{
	ComPtr<ID3DBlob> SigBlob;
	ComPtr<ID3DBlob> ErrorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&Desc, D3D_ROOT_SIGNATURE_VERSION_1, &SigBlob, &ErrorBlob);
	if (FAILED(hr))
	{
		return nullptr;
	}

	ComPtr<ID3D12RootSignature> RootSig;
	Device->CreateRootSignature(0, SigBlob->GetBufferPointer(), SigBlob->GetBufferSize(), IID_PPV_ARGS(RootSig.GetAddressOf()));
	return RootSig;
}

RayGenRootSigDesc DXRRenderer::CreateRayGenRootDesc()
{
	RayGenRootSigDesc Desc;
	Desc.Ranges.resize(2);

	Desc.Ranges[0].BaseShaderRegister = 0;
	Desc.Ranges[0].NumDescriptors = 1;
	Desc.Ranges[0].RegisterSpace = 0;
	Desc.Ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	Desc.Ranges[0].OffsetInDescriptorsFromTableStart = 0;

	Desc.Ranges[1].BaseShaderRegister = 0;
	Desc.Ranges[1].NumDescriptors = 1;
	Desc.Ranges[1].RegisterSpace = 0;
	Desc.Ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	Desc.Ranges[1].OffsetInDescriptorsFromTableStart = 1;

	Desc.RootParams.resize(1);
	Desc.RootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	Desc.RootParams[0].DescriptorTable.NumDescriptorRanges = 2;
	Desc.RootParams[0].DescriptorTable.pDescriptorRanges = Desc.Ranges.data();

	Desc.Desc.NumParameters = 1;
	Desc.Desc.pParameters = Desc.RootParams.data();
	Desc.Desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	return Desc;
}

void DXRRenderer::CreateRayTracingPipelineState()
{
	array<D3D12_STATE_SUBOBJECT, 10> SubObjects;
	UINT32 Index = 0;

	DXILLib DxilLib = CreateDXILLib();
	SubObjects[Index++] = DxilLib.StateSubObj;

	HitProgram HitProg(nullptr, EntryClosestHitShader, EntryIntersectionSphereShader, EntryHitGroup);
	SubObjects[Index++] = HitProg.SubObject;

	LocalRootSignature LocalRootSig(Device.Get(), CreateRayGenRootDesc().Desc);
	SubObjects[Index] = LocalRootSig.SubObject;

	ExportAssociation RootAssociation(&EntryRayGenShader, 1, &(SubObjects[Index++]));
	SubObjects[Index++] = RootAssociation.SubObject;

	D3D12_ROOT_SIGNATURE_DESC EmptyEsc = {};
	EmptyEsc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	LocalRootSignature HitMissRootSig(Device.Get(), EmptyEsc);
	SubObjects[Index] = HitMissRootSig.SubObject;

	const WCHAR* MissHitExportName[] = { EntryMissShader, EntryClosestHitShader };
	ExportAssociation MissHitRootAssociation(MissHitExportName, 2, &(SubObjects[Index++]));
	SubObjects[Index++] = MissHitRootAssociation.SubObject;

	ShaderConfig ShaderCfg(sizeof(float) * 3, sizeof(float)*5);
	SubObjects[Index] = ShaderCfg.SubObject;

	const WCHAR* ShaderExport[] = { EntryMissShader, EntryClosestHitShader, EntryRayGenShader, EntryIntersectionSphereShader };
	ExportAssociation CfgAssociation(ShaderExport, 4, &(SubObjects[Index++]));
	SubObjects[Index++] = CfgAssociation.SubObject;

	PipelineConfig PipCfg(1);
	SubObjects[Index++] = PipCfg.SubObject;

	GlobalRootSignature GlobalRootSig(Device.Get(), {});
	EmptyRootSig = GlobalRootSig.RootSig;
	SubObjects[Index++] = GlobalRootSig.SubObject;

	D3D12_STATE_OBJECT_DESC Desc;
	Desc.NumSubobjects = Index;
	Desc.pSubobjects = SubObjects.data();
	Desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

	D3D_CALL(Device->CreateStateObject(&Desc, IID_PPV_ARGS(&PipelineStateObject)));

}

void DXRRenderer::CreateShaderTable()
{
	ShaderTableEntrySize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	ShaderTableEntrySize += 8;
	ShaderTableEntrySize = ALIGN_TO(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, ShaderTableEntrySize);
	UINT32 ShaderTableSize = ShaderTableEntrySize * 3;
	ShaderTable = CreateBuffer(ShaderTableSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, UploadHeapProps);

	UINT8* Data;
	ShaderTable->Map(0, nullptr, (void**)&Data);
	ComPtr<ID3D12StateObjectProperties> SOProps;
	PipelineStateObject->QueryInterface(IID_PPV_ARGS(&SOProps));
	memcpy(Data, SOProps->GetShaderIdentifier(EntryRayGenShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

	*(UINT64*)(Data + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = SrvUavHeap->GetGPUDescriptorHandleForHeapStart().ptr;

	memcpy(Data + ShaderTableEntrySize, SOProps->GetShaderIdentifier(EntryMissShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

	UINT8* HitEntry = Data + ShaderTableEntrySize * 2;
	memcpy(HitEntry, SOProps->GetShaderIdentifier(EntryHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

	ShaderTable->Unmap(0, nullptr);
}

void DXRRenderer::CreateShaderResource()
{
	D3D12_RESOURCE_DESC ResDesc = {};
	ResDesc.DepthOrArraySize = 1;
	ResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	ResDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ResDesc.Width = BackbufferWidth;
	ResDesc.Height = BackbufferHeight;
	ResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	ResDesc.MipLevels = 1;
	ResDesc.SampleDesc.Count = 1;
	D3D_CALL(Device->CreateCommittedResource(&DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &ResDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&OutputResource)));

	SrvUavHeap = CreateDescriptorHeap(2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	
	D3D12_UNORDERED_ACCESS_VIEW_DESC UavDesc = {};
	UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	Device->CreateUnorderedAccessView(OutputResource.Get(), nullptr, &UavDesc, SrvUavHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SrvDesc.RaytracingAccelerationStructure.Location = TopLevelAS->GetGPUVirtualAddress();
	D3D12_CPU_DESCRIPTOR_HANDLE SrvHandle = SrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	SrvHandle.ptr += Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	Device->CreateShaderResourceView(nullptr, &SrvDesc, SrvHandle);

}

void DXRRenderer::Exit()
{
	FenceValue++;
	CommandQueue->Signal(RenderFence.Get(), FenceValue);
	RenderFence->SetEventOnCompletion(FenceValue, FenceEvent);
	WaitForSingleObject(FenceEvent, INFINITE);
}