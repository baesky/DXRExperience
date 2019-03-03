#include "ProcedureLib.hlsli"

struct ProceduralPrimitiveAttributes
{
	float3 Normal;
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

RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

StructuredBuffer<PrimitiveInstancePerFrameBuffer> gAABBPrimAttr : register(t1);
ConstantBuffer<PrimitiveConstantBuffer> MaterialCB : register(b0);
ConstantBuffer<PrimitiveInstanceConstantBuffer> InstanceConstCB: register(b1);

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
	uint RayDepth;
};

float4 TraceScene(in RayDesc Ray, in uint CurrentRayDepth)
{
	if (CurrentRayDepth >= MAX_RAY_RECURSION_DEPTH)
	{
		return float4(1, 0, 0, 1);
	}

	RayPayload Payload = { float4(0, 0, 1, 1), CurrentRayDepth + 1 };

	TraceRay(gRtScene, 
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 
		0xFF, 
		0, 
		0, 
		0, 
		Ray, 
		Payload);

	return Payload.Color;
}

[shader("raygeneration")]
void RayGen()
{  
    uint3 TIndex = DispatchRaysIndex();
	uint3 TDim = DispatchRaysDimensions();

	float2 Coord = float2(TIndex.xy);
	float2 Dims = float2(TDim.xy);
	float2 d = ((Coord / Dims)*2.0f - 1.0f);
	float AspectRatio = Dims.x / Dims.y;

	RayDesc Ray;
	Ray.Origin = float3(0, 0, -11);
	Ray.Direction = normalize(float3(d.x * AspectRatio, -d.y, 1));

	Ray.TMin = 0;
	Ray.TMax = 10000;

	float4 Col = TraceScene(Ray, 0);
	Col.xyz = linearToSrgb(Col.xyz);
	gOutput[TIndex.xy] = float4(Col.xyz, 1);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
	float t = 0.5f*(WorldRayDirection().y + 1.0f);
	payload.Color = float4((1.0f - t) * float3(1.0, 1.0, 1.0) + t * float3(0.5, 0.7, 1.0), 1.0);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload Payload, in ProceduralPrimitiveAttributes Attr)
{
	float3 Range = 0.5f * (Attr.Normal + float3(1.0f, 1.0f, 1.0f));
	Payload.Color = float4(Range,1);

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
	const int N = 3;
	float3 centers[N] =
	{
		float3(-0.3, -0.3, -0.3),
		float3(0, 0, 0),
		float3(0.35,0.35, 0.0)
	};
	float  radii[N] = { 0.6, 0.3, 0.15 };
	bool hitFound = false;

	//
	// Test for intersection against all spheres and take the closest hit.
	//
	//thit = RayTCurrent();

	//// test against all spheres
	//for (int i = 0; i < N; i++)
	//{
	//	float _thit;
	//	float _tmax;
	//	ProceduralPrimitiveAttributes _attr;
	//	if (RaySphereIntersectionTest(Ray, _thit, _tmax, _attr, centers[i], radii[i]))
	//	{
	//		if (_thit < thit)
	//		{
	//			thit = _thit;
	//			attr = _attr;
	//			hitFound = true;
	//		}
	//	}
	//}

	if (HitSphere(centers[1], 3.0f, Ray, Attr.Normal, thit))
	{
		hitFound = true;
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
	if (RaySpheresIntersectionTest(LocalRay, thit, Attr))
	{
		//PrimitiveInstancePerFrameBuffer aabbAttribute = gAABBPrimAttr[InstanceConstCB.InstanceIndex];
		//Attr.normal = mul(Attr.normal, (float3x3) aabbAttribute.LocalToBLAS);
		//Attr.normal = normalize(mul((float3x3) ObjectToWorld3x4(), Attr.normal));

		ReportHit(thit, /*hitKind*/ 0, Attr);
	}
}