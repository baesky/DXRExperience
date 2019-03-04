#ifndef PROCEDURELIB_H
#define PROCEDURELIB_H

#define T_MIN 0
#define T_MAX 3.402823466e+38

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