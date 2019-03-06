#include "ProcedureLib.hlsli"

struct ProceduralPrimitiveAttributes
{
	float3 Normal;
	float4 Color;
	float Atten;
};

struct PrimitiveInstancePerFrameBuffer
{
	float4x4 LocalToBLAS;
	float4x4 BLASToLocal;   
};

struct PrimitiveConstantBuffer
{
	float4 Albedo;
};

struct PrimitiveInstanceConstantBuffer
{
	uint InstanceIndex;
};

struct SystemValBuffer
{
	uint GFrameCounter;
};

RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

StructuredBuffer<PrimitiveInstancePerFrameBuffer> gAABBPrimAttr : register(t1);
ConstantBuffer<PrimitiveConstantBuffer> MaterialCB : register(b0 , space0);
ConstantBuffer<PrimitiveInstanceConstantBuffer> InstanceConstCB: register(b1, space0);

ConstantBuffer<SystemValBuffer> SysVal : register(b0, space1);

#define MAX_RAY_RECURSION_DEPTH 3

float3 linearToSrgb(float3 c)
{
    // Based on http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    float3 sq1 = sqrt(c);
    float3 sq2 = sqrt(sq1);
    float3 sq3 = sqrt(sq2);
    float3 srgb = 0.662002687 * sq1 + 0.684122060 * sq2 - 0.323583601 * sq3 - 0.0225411470 * c;
    return srgb;
}

struct RayPayload
{
	float4 Color;
	float3 HitPos;
	float3 HitNormal;
	float HitNum;
};

float4 TraceScene(in RayDesc Ray, in RayDesc OriginRay)
{

	float4 CurrAtten = float4(1.0f,1.0f,1.0f,1.0f);
	for (int i = 0; i < MAX_RAY_RECURSION_DEPTH; ++i)
	{

		RayPayload Payload = { float4(0, 0, 0, 0),float3(0, 0, 0), float3(0, 0, 0),0.0 };
		TraceRay(gRtScene,
			RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
			0xFF,
			0,
			0,
			0,
			Ray,
			Payload);
		if (Payload.HitNum > 0.0f)
		{
			Ray.Origin = Payload.HitPos;

			float3 Tar = Payload.HitPos + Payload.HitNormal + hash32(Rand(DispatchRaysIndex()) + hash21(DispatchRaysIndex().x*sin(SysVal.GFrameCounter) + DispatchRaysIndex().y*cos(SysVal.GFrameCounter) + i));
			Ray.Direction = Tar - Payload.HitPos;

			CurrAtten *= Payload.Color;
		}
		else
		{
			float t = 0.5*(OriginRay.Direction.y + 1.0f);

			return CurrAtten * float4((1.0f - t) * float3(1.0, 1.0, 1.0) + t * float3(0.5, 0.7, 1.0), 1.0);

		}

	}

	return float4(0,0,0,0);
}

[shader("raygeneration")]
void RayGen()
{  
    uint3 TIndex = DispatchRaysIndex();
	uint3 TDim = DispatchRaysDimensions();

	float2 Coord = float2(TIndex.xy);
	float2 Dims = float2(TDim.xy);
	
	float AspectRatio = Dims.x / Dims.y;

	RayDesc Ray;
	Ray.Origin = float3(0 , sin(SysVal.GFrameCounter * 0.0001f) * 0.5f + 0.5, -1);//
	Ray.TMin = 0;
	Ray.TMax = 10000;
	float3 TempDir = normalize(float3(0, 0, 1) - Ray.Origin);
	float3 Right = cross(float3(0, 1, 0), TempDir);
	float3 Up = cross(TempDir, Right);
	float4 Col = float4(0, 0, 0, 0);
	for (int i = 0; i < MAX_RAY_RECURSION_DEPTH; i++)
	{
		float2 d = (((Coord+ hash21(sin(SysVal.GFrameCounter))) / Dims) * 2.0f - 1.0f);

		Ray.Direction = normalize(d.x * AspectRatio * Right - d.y * Up + 2.0 * TempDir);
		Col += TraceScene(Ray, Ray);
	}
	Col.xyz /= float(MAX_RAY_RECURSION_DEPTH);
	
	Col.xyz = linearToSrgb(Col.xyz);
	gOutput[TIndex.xy] = float4(Col.xyz, 1);
}

[shader("miss")]
void Miss(inout RayPayload Payload)
{
	float t = 0.5*(WorldRayDirection().y + 1.0f);

	Payload.Color = float4((1.0f - t) * float3(1.0, 1.0, 1.0) + t * float3(0.5, 0.7, 1.0), 1.0);
	Payload.HitNum = 0.0f;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload Payload, in ProceduralPrimitiveAttributes Attr)
{
	/*float3 Range = 0.5f * (Attr.Normal + float3(1.0f, 1.0f, 1.0f));
	Payload.Color = float4(Range,1);*/
	Payload.Color = Attr.Color;
	Payload.HitNum = Attr.Atten;
	Payload.HitPos = HitWorldPosition();
	Payload.HitNormal = Attr.Normal;
}

//RayDesc GetRayInAABBPrimLocalSpace()
//{
//	PrimitiveInstancePerFrameBuffer Attr = gAABBPrimAttr[InstanceConstCB.InstanceIndex];
//
//	// Retrieve a ray origin position and direction in bottom level AS space 
//	// and transform them into the AABB primitive's local space.
//	RayDesc Ray;
//	Ray.origin = mul(float4(ObjectRayOrigin(), 1), Attr.BLASToLocal).xyz;
//	Ray.direction = mul(ObjectRayDirection(), (float3x3) Attr.BLASToLocal);
//	return Ray;
//}

bool RaySpheresIntersectionTest(in RayDesc Ray, out float thit, out ProceduralPrimitiveAttributes Attr)
{
	const int N = 2;
	float3 centers[N] =
	{
		float3(0, -100.5, 1),
		float3(0, 0, 1),
		//float3(0.35,0.35, 0.0)
	};

	float4 Colors[N] =
	{
		float4(0.8,0.8,0.0,1.0),
		float4(0.8,0.3,0.3,1.0)
	};

	float  radii[N] = { 100.0f, 0.5f/*, 0.15*/ };
	bool hitFound = false;

	//
	// Test for intersection against all spheres and take the closest hit.
	//
	thit = RayTCurrent();

	//// test against all spheres
	for (int i = 0; i < N; i++)
	{
		float _thit;
		float _tmax;
		ProceduralPrimitiveAttributes _attr;
		
		if (HitSphere(centers[i], radii[i], Ray, _attr.Normal, _thit))
		{
			if (_thit < thit)
			{
				_attr.Color = Colors[i];
				thit = _thit;
				Attr = _attr;
				
				hitFound = true;
			}
			
		}
	}

	return hitFound;
}

[shader("intersection")]
void IntersectionAnalyticPrimitive()
{
	//RayDesc LocalRay = GetRayInAABBPrimLocalSpace();
	RayDesc LocalRay;
	LocalRay.Origin = WorldRayOrigin();
	LocalRay.Direction = WorldRayDirection();

	float thit;
	ProceduralPrimitiveAttributes Attr;
	Attr.Atten = 0.0f;
	if (RaySpheresIntersectionTest(LocalRay, thit, Attr))
	{
		//PrimitiveInstancePerFrameBuffer aabbAttribute = gAABBPrimAttr[InstanceConstCB.InstanceIndex];
		//Attr.normal = mul(Attr.normal, (float3x3) aabbAttribute.LocalToBLAS);
		//Attr.normal = normalize(mul((float3x3) ObjectToWorld3x4(), Attr.normal));
		Attr.Atten = 1.0f;
		ReportHit(thit, /*hitKind*/ 0, Attr);
	}
}