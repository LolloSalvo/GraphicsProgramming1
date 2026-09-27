//==============================================================
//  Graphics Programming 1 (2026-2027)
//  Authors : Matthieu Delaere
//  Copyright (c) 2026 Matthieu Delaere. All rights reserved.
//==============================================================
#ifndef INTERSECTIONS_HEADER
#define INTERSECTIONS_HEADER

//--- Standard Includes ---
#include <cassert>

//--- Framework Includes ---
#include <primitives.h>
#include <matrix.h>

namespace gfx
{
	//--- Intersection Tests ---
	[[maybe_unused]]
	static bool HitTestSphere(const Sphere& sphere, const Ray& ray,
		RayHitRecord& hit_record, const bool ignore_hit_record = false)
	{
		//TODO
		
		Vector3 directionVector{ ray.direction };
		Vector3 l{ sphere.origin - ray.origin }; // Distance between the Ray origin and the sphere origin

		float t_ca{ Vector3::Dot(l,directionVector) }; // Projection of the vector L on the direction Vector of the Ray
		float  od{ sqrtf(Vector3::Dot(l,l) - t_ca * t_ca) }; // Height of the right triangle formed by L and the proj of it on the direction of the Ray

		if (od > sphere.radius) return false;

		float t_hc{ sqrtf((sphere.radius * sphere.radius) - (od * od)) }; // Using the now found height od we can use the same angle and the radius to find half of the length of the part of the ray passing through the sphere
		
		float t0{ t_ca - t_hc }; //This is finally the distance between the ray origin and the first hit of the ray on the sphere surface

		if (t0 > ray.min && t0 < ray.max && ignore_hit_record == false) 
		{
			hit_record.t = t0;
			hit_record.ray = ray;
			return true;
		}


		/*assert(false && "Not Implemented");
		(void)sphere; (void)ray; (void)hit_record; (void)ignore_hit_record;*/
		return false;
	}

	[[maybe_unused]]
	static bool HitTestPlane(const Plane& plane, const Ray& ray, 
		RayHitRecord& hit_record, const bool ignore_hit_record = false)
	{
		//TODO
		Vector3 directionVerctor{ ray.direction };

		float  angularProportion{ Vector3::Dot(ray.direction, plane.normal) };
		if (angularProportion < 0.f)
		{
			float t{ (Vector3::Dot((plane.origin - ray.origin), plane.normal) / angularProportion) };

			Vector3 hitPoint{ ray.origin + ray.direction * t };
			Vector3 b{ Vector3::Cross(plane.normal, plane.tangent) };

			if (plane.half_extent.has_value())
			{
				float t1{ Vector3::Dot((hitPoint - plane.origin), plane.tangent) };
				float b1{ Vector3::Dot((hitPoint - plane.origin), b) };

				if (abs(t1) <= plane.half_extent.value().Magnitude() && abs(b1) <= plane.half_extent.value().Magnitude())
				{
					if (t > ray.min && t < ray.max && ignore_hit_record == false)
					{
						hit_record.t = t;
						hit_record.ray = ray;
						return true;
					}
				}

			}
			else
			{
				if (t > ray.min && t < ray.max && ignore_hit_record == false)
				{
					hit_record.t = t;
					hit_record.ray = ray;
					return true;
				}

			}

		}



		/*assert(false && "Not Implemented");
		(void)plane; (void)ray; (void)hit_record; (void)ignore_hit_record;*/
		return false;
	}

	[[maybe_unused]]
	static bool HitTestTriangle(const Triangle& triangle, const Ray& ray, 
		RayHitRecord& hit_record, const bool ignore_hit_record = false)
	{
		//TODO
		assert(false && "Not Implemented");
		(void)triangle; (void)ray; (void)hit_record; (void)ignore_hit_record;
		return false;
	}

	[[maybe_unused]]
	static bool HitTestAABB(const AABB& aabb, const Ray& ray)
	{
		//TODO
		assert(false && "Not Implemented");
		(void)aabb; (void)ray;
		return false;
	}
}
#endif //INTERSECTIONS_HEADER