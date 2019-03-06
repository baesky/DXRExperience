#pragma once
#include "../Externals/DXCAPI/dxcapi.use.h"
#include <fstream>
#include <sstream>
#include <vector>

#define D3D_CALL(a) {HRESULT hr_ = a; if(FAILED(hr_)) { OutputDebugStringA(#a); }}

static dxc::DxcDllSupport GDxcDllHelper;

template<class BlotType>
std::string ConvertBlobToString(BlotType* pBlob)
{
	std::vector<char> infoLog(pBlob->GetBufferSize() + 1);
	memcpy(infoLog.data(), pBlob->GetBufferPointer(), pBlob->GetBufferSize());
	infoLog[pBlob->GetBufferSize()] = 0;
	return std::string(infoLog.data());
}

struct DXILLib
{
	DXILLib(ComPtr<ID3DBlob> Blob, const WCHAR* EntryPoint[], UINT32 EntryPointNum) : ShaderBlob(Blob)
	{
		StateSubObj.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		StateSubObj.pDesc = &DXILLibDesc;
		DXILLibDesc = {};
		
		ExportDescs.resize(EntryPointNum);
		ExportNames.resize(EntryPointNum);
		if (Blob)
		{
			DXILLibDesc.DXILLibrary.pShaderBytecode = Blob->GetBufferPointer();
			DXILLibDesc.DXILLibrary.BytecodeLength = Blob->GetBufferSize();
			DXILLibDesc.NumExports = EntryPointNum;
			DXILLibDesc.pExports = ExportDescs.data();

			for (UINT32 Idx = 0; Idx < EntryPointNum; ++Idx)
			{
				ExportNames[Idx] = EntryPoint[Idx];
				ExportDescs[Idx].Name = ExportNames[Idx].c_str();
				ExportDescs[Idx].Flags = D3D12_EXPORT_FLAG_NONE;
				ExportDescs[Idx].ExportToRename = nullptr;
			}
		}
	}

	DXILLib():DXILLib(nullptr, nullptr, 0){}

	D3D12_DXIL_LIBRARY_DESC DXILLibDesc = {};
	D3D12_STATE_SUBOBJECT StateSubObj = {};
	ComPtr<ID3DBlob> ShaderBlob;
	std::vector<D3D12_EXPORT_DESC> ExportDescs;
	std::vector<std::wstring> ExportNames;
};

static const WCHAR* EntryRayGenShader = L"RayGen";
static const WCHAR* EntryMissShader = L"Miss";
static const WCHAR* EntryClosestHitShader = L"ClosestHit";
static const WCHAR* EntryHitGroup = L"HitGroup";
static const WCHAR* EntryIntersectionSphereShader = L"IntersectionAnalyticPrimitive";

ComPtr<ID3DBlob> CompileShader(const wchar_t* FileName, const wchar_t* TargetStr)
{
	GDxcDllHelper.Initialize();
	ComPtr<IDxcCompiler> Compiler;
	ComPtr<IDxcLibrary> Library;
	GDxcDllHelper.CreateInstance(CLSID_DxcCompiler, Compiler.GetAddressOf());
	GDxcDllHelper.CreateInstance(CLSID_DxcLibrary, Library.GetAddressOf());

	std::ifstream ShaderFile(FileName);
	if (!ShaderFile.good())
		return nullptr;

	std::stringstream StrStream;
	StrStream << ShaderFile.rdbuf();
	std::string Shader = StrStream.str();

	ComPtr<IDxcBlobEncoding> TextBlob;
	Library->CreateBlobWithEncodingFromPinned((LPBYTE)Shader.c_str(), (UINT32)Shader.size(), 0, &TextBlob);
	ComPtr<IDxcIncludeHandler> IncHandler;
	if (SUCCEEDED(Library->CreateIncludeHandler(&IncHandler)))
	{
		ComPtr<IDxcBlob> IncBlob;
		HRESULT hh = IncHandler->LoadSource(L"Shaders/ProcedureLib.hlsli", &IncBlob);
		if (FAILED(hh))
		{
			return nullptr;
		}
	}

	ComPtr<IDxcOperationResult> Result;
	Compiler->Compile(TextBlob.Get(), FileName, L"", TargetStr, nullptr, 0, nullptr, 0, IncHandler.Get(), &Result);

	HRESULT ResultCode;
	Result->GetStatus(&ResultCode);
	if (FAILED(ResultCode))
	{
		ComPtr<IDxcBlobEncoding> Error;
		Result->GetErrorBuffer(&Error);
		std::string log = ConvertBlobToString(Error.Get());
		OutputDebugStringA(log.c_str());
		return nullptr;
	}

	ComPtr<IDxcBlob> Blob;
	Result->GetResult(&Blob);
	ComPtr<ID3DBlob> D3DBlob;
	Blob.As(&D3DBlob);
	return D3DBlob;
}

DXILLib CreateDXILLib()
{
	ComPtr<ID3DBlob> DXILLibBlob = CompileShader(L"Shaders/RayTracing.hlsl", L"lib_6_3");
	const WCHAR* EntryPoints[] = { EntryRayGenShader , EntryMissShader , EntryClosestHitShader, EntryIntersectionSphereShader };
	return DXILLib(DXILLibBlob, EntryPoints, 4);
}

struct HitProgram
{
	HitProgram(LPCWSTR AnyHitShaderExport, LPCWSTR CloseHitShaderExport, LPCWSTR IntersectionShaderExport,const std::wstring& Name) : ExportName(Name)
	{
		Desc = {};
		Desc.ClosestHitShaderImport = CloseHitShaderExport;
		Desc.AnyHitShaderImport = AnyHitShaderExport;
		Desc.HitGroupExport = ExportName.c_str();
		Desc.IntersectionShaderImport = IntersectionShaderExport;
		Desc.Type = D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;

		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		SubObject.pDesc = &Desc;
	}

	std::wstring ExportName;
	D3D12_HIT_GROUP_DESC Desc;
	D3D12_STATE_SUBOBJECT SubObject;
};

struct ExportAssociation
{
	ExportAssociation(const WCHAR* ExportNames[], UINT32 ExportCount, const D3D12_STATE_SUBOBJECT* SubObjToAssociate)
	{
		Association.NumExports = ExportCount;
		Association.pExports = ExportNames;
		Association.pSubobjectToAssociate = SubObjToAssociate;

		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		SubObject.pDesc = &Association;
	}

	D3D12_STATE_SUBOBJECT SubObject = {};
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION Association = {};
};

ComPtr<ID3D12RootSignature> CreateRootSig(ID3D12Device5* Device, const D3D12_ROOT_SIGNATURE_DESC& Desc)
{
	ComPtr<ID3DBlob> SigBlob;
	ComPtr<ID3DBlob> ErrorBlob;
	D3D_CALL(D3D12SerializeRootSignature(&Desc, D3D_ROOT_SIGNATURE_VERSION_1, &SigBlob, &ErrorBlob));

	ComPtr<ID3D12RootSignature> RootSig;
	D3D_CALL(Device->CreateRootSignature(0, SigBlob->GetBufferPointer(), SigBlob->GetBufferSize(), IID_PPV_ARGS(&RootSig)));
	return RootSig;
};

struct LocalRootSignature
{
	LocalRootSignature(ID3D12Device5* Device, const D3D12_ROOT_SIGNATURE_DESC& Desc)
	{
		RootSig = CreateRootSig(Device, Desc);
		Interface = RootSig.Get();
		SubObject.pDesc = &Interface;
		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	}

	LocalRootSignature(ID3D12Device5* Device)
	{
		D3D12_ROOT_PARAMETER RootParams[2];
		RootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		RootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		RootParams[0].Constants.Num32BitValues = sizeof(float) * 4;
		RootParams[0].Constants.RegisterSpace = 0;
		RootParams[0].Constants.ShaderRegister = 0;

		RootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		RootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		RootParams[1].Constants.Num32BitValues = sizeof(int);
		RootParams[1].Constants.RegisterSpace = 0;
		RootParams[1].Constants.ShaderRegister = 1;

		D3D12_ROOT_SIGNATURE_DESC RootSigDesc = {};

		RootSigDesc.NumParameters = 2;
		RootSigDesc.pParameters = RootParams;
		RootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

		RootSig = CreateRootSig(Device, RootSigDesc);
		Interface = RootSig.Get();
		SubObject.pDesc = &Interface;
		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	}

	ComPtr<ID3D12RootSignature> RootSig;
	ID3D12RootSignature* Interface = nullptr;
	D3D12_STATE_SUBOBJECT SubObject = {};
};

struct GlobalRootSignature
{
	GlobalRootSignature(ID3D12Device5* Device, D3D12_ROOT_SIGNATURE_DESC& Desc)
	{
		D3D12_DESCRIPTOR_RANGE DescRanges[2];
		DescRanges[0].BaseShaderRegister = 0;
		DescRanges[0].NumDescriptors = 1;
		DescRanges[0].RegisterSpace = 0;
		DescRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		DescRanges[0].OffsetInDescriptorsFromTableStart = 0;

		DescRanges[1].BaseShaderRegister = 0;
		DescRanges[1].NumDescriptors = 1;
		DescRanges[1].RegisterSpace = 0;
		DescRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		DescRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER RootParam[3];
		RootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		RootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		RootParam[0].DescriptorTable.NumDescriptorRanges = 1;
		RootParam[0].DescriptorTable.pDescriptorRanges = &DescRanges[0];

		RootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		RootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		RootParam[1].DescriptorTable.NumDescriptorRanges = 1;
		RootParam[1].DescriptorTable.pDescriptorRanges = &DescRanges[1];

		RootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		RootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		RootParam[2].Constants.Num32BitValues = sizeof(UINT);
		RootParam[2].Constants.RegisterSpace = 1;
		RootParam[2].Constants.ShaderRegister = 0;

		Desc.NumParameters = 3;
		Desc.pParameters = RootParam;

		RootSig = CreateRootSig(Device, Desc);
		Interface = RootSig.Get();
		SubObject.pDesc = &Interface;
		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	}
	ComPtr<ID3D12RootSignature> RootSig;
	ID3D12RootSignature* Interface = nullptr;
	D3D12_STATE_SUBOBJECT SubObject = {};
};

struct ShaderConfig
{
	ShaderConfig(UINT32 MaxAttributeSizeInBytes, UINT32 MaxPayloadSizeInBytes)
	{
		ShaderCfg.MaxAttributeSizeInBytes = MaxAttributeSizeInBytes;
		ShaderCfg.MaxPayloadSizeInBytes = MaxPayloadSizeInBytes;
		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
		SubObject.pDesc = &ShaderCfg;
	}

	D3D12_RAYTRACING_SHADER_CONFIG ShaderCfg = {};
	D3D12_STATE_SUBOBJECT SubObject = {};
};

struct PipelineConfig
{
	PipelineConfig(UINT32 MaxTraceRecursionDepth)
	{
		PipelineCfg.MaxTraceRecursionDepth = MaxTraceRecursionDepth;
		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
		SubObject.pDesc = &PipelineCfg;
	}

	D3D12_RAYTRACING_PIPELINE_CONFIG PipelineCfg = {};
	D3D12_STATE_SUBOBJECT SubObject = {};
};