#ifndef PROCEDURELIB_H
#define PROCEDURELIB_H

#define T_MIN 0
#define T_MAX 3.402823466e+38

//
float Rand(float2 co) {
	return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}

#define HASHSCALE3 float3(.1031, .1030, .0973)
float3 hash32(float2 p)
{
	float3 p3 = frac(float3(p.xyx) * HASHSCALE3);
	p3 += dot(p3, p3.yxz + 19.19);
	return frac((p3.xxy + p3.yzz) * p3.zyx);
}
float2 hash21(float p)
{
	float3 p3 = frac(float3(p,p,p) * HASHSCALE3);
	p3 += dot(p3, p3.yzx + 19.19);
	return frac((p3.xx + p3.yz) * p3.zy);

}

float3 PointAt(float3 RayOrigin, float3 RayDir, float t)
{
	return RayOrigin + RayDir * t;
}

float3 HitWorldPosition()
{
	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

bool HitSphere(in float3 Center, float Radius, in RayDesc Ray, out float3 Normal, out float Dist)
{
	float3 oc = Ray.Origin - Center;
	float a = dot(Ray.Direction, Ray.Direction);
	float b = 2.0f * dot(oc, Ray.Direction);
	float c = dot(oc, oc) - Radius * Radius;
	float Discriminat = b * b - 4.0f * a *c;
	
	bool bHit = (Discriminat > 0.0f);

	if (bHit)
	{
		float temp = 0.5 * (-b - sqrt(Discriminat)) / a;
		if (temp < T_MAX && temp > T_MIN) 
		{
			Dist = temp;
			float3 D = PointAt(Ray.Origin, Ray.Direction, Dist);
			Normal = (D - Center) / Radius;
			return bHit;
		}
		temp = 0.5 * (-b + sqrt(Discriminat)) / a;
		if (temp < T_MAX && temp > T_MIN) 
		{
			Dist = temp;
			float3 D = PointAt(Ray.Origin, Ray.Direction, Dist);
			Normal = (D - Center) / Radius;
			return bHit;
		}
	}

	return false;
}

#endif