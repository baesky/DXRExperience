#ifndef PROCEDURELIB_H
#define PROCEDURELIB_H

bool HitSphere(in float3 Center, float Radius, in RayDesc Ray)
{
	float3 oc = Ray.Origin - Center;
	float a = dot(Ray.Direction, Ray.Direction);
	float b = 2.0f * dot(oc, Ray.Direction);
	float c = dot(oc, oc) - Radius * Radius;
	float Discriminat = b * b - 4.0f * a *c;
	return Discriminat > 0.0f;
}

#endif