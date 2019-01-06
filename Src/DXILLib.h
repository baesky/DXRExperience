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
	ComPtr<IDxcOperationResult> Result;
	Compiler->Compile(TextBlob.Get(), FileName, L"", TargetStr, nullptr, 0, nullptr, 0, nullptr, &Result);

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
	ComPtr<ID3DBlob> DXILLibBlob = CompileShader(L"../Shaders/RayTracing.hlsl", L"lib_6_3");
	const WCHAR* EntryPoints[] = { EntryRayGenShader , EntryMissShader , EntryClosestHitShader };
	return DXILLib(DXILLibBlob, EntryPoints, 3);
}

struct HitProgram
{
	HitProgram(LPCWSTR AnyHitShaderExport, LPCWSTR CloseHitShaderExport, const std::wstring& Name) : ExportName(Name)
	{
		Desc = {};
		Desc.ClosestHitShaderImport = CloseHitShaderExport;
		Desc.AnyHitShaderImport = AnyHitShaderExport;
		Desc.HitGroupExport = ExportName.c_str();

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

	ComPtr<ID3D12RootSignature> RootSig;
	ID3D12RootSignature* Interface = nullptr;
	D3D12_STATE_SUBOBJECT SubObject = {};
};

struct GlobalRootSignature
{
	GlobalRootSignature(ID3D12Device5* Device, const D3D12_ROOT_SIGNATURE_DESC& Desc)
	{
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