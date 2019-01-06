RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

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
	float3 Color;
};

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
	Ray.Origin = float3(0, 0, -2);
	Ray.Direction = normalize(float3(d.x * AspectRatio, -d.y, 1));

	Ray.TMin = 0;
	Ray.TMax = 100000;

	RayPayload Payload;
	TraceRay(gRtScene, 0, 0xFF, 0, 0, 0, Ray, Payload);
	float3 Col = linearToSrgb(Payload.Color);
	gOutput[TIndex.xy] = float4(Col, 1);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
	float t = 0.5f*(WorldRayDirection().y + 1.0f);
	payload.Color = (1.0f - t)*float3(1.0, 1.0, 1.0) + t * float3(0.5, 0.7, 1.0);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	float3 Barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
	const float3 A = float3(1, 0, 0);
	const float3 B = float3(0, 1, 0);
	const float3 C = float3(0, 0, 1);
	
	payload.Color = A * Barycentrics.x + B * Barycentrics.y + C * Barycentrics.z;

}