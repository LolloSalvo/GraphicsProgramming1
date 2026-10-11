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
#include <assimp/defs.h>
#include <assimp/MathFunctions.h>

namespace gfx
{
	//--- Intersection Tests ---
	[[maybe_unused]]
	static bool HitTestSphere(const Sphere& sphere, const Ray& ray,
		RayHitRecord& hit_record, const bool ignore_hit_record = false)
	{
		//Calculation to see if the ray hit the sphere, returning true if the ray hit the sphere and the value of t0 or t1 depending on which side of the sphere was hit
		
		Vector3 directionVector{ ray.direction };
		Vector3 l{ sphere.origin - ray.origin };

		float t_ca{ Vector3::Dot(l,directionVector) };
		float  od_squared{ Vector3::Dot(l,l) - t_ca * t_ca };
		if (od_squared < 0.0f) od_squared = 0.0f;

		if (od_squared > sphere.radius * sphere.radius) return false;

		float t_hc{ sqrtf((sphere.radius * sphere.radius) - od_squared) }; // Using the now found height od we can use the same angle and the radius to find half of the length of the part of the ray passing through the sphere
		
		float t0{ t_ca - t_hc };
		float t1{ t_ca + t_hc };

		float valid_t{};

		if (t0 > ray.min && t0 < ray.max)
		{
			valid_t = t0;
		}
		else if (t1 > ray.min && t1 < ray.max)
		{
			valid_t = t1;
		}
		else
		{
			return false;
		}

		if (ignore_hit_record == false) 
		{
			hit_record.t = valid_t;
			hit_record.ray = ray;
		}

		return true;
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

				if (abs(t1) <= plane.half_extent.value().x && abs(b1) <= plane.half_extent.value().y)
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

		return false;
	}

	[[maybe_unused]]
	static bool HitTestTriangle(const Triangle& triangle, const Ray& ray, 
		RayHitRecord& hit_record, const bool ignore_hit_record = false)
	{
		float n_dot_r{ Vector3::Dot(triangle.normal, ray.direction) };
		if (abs(n_dot_r) < ai_epsilon)
		{
			return false;
		}

		bool is_face_visible{ false };

		switch (triangle.cull_mode)
		{
		case CullMode::kBackFaceCulling:
			{
				if (n_dot_r < 0.f)
				{
					is_face_visible = true;
				}
				break;
			}
		case CullMode::kFrontFaceCulling:
			{
				if (n_dot_r > 0.f)
				{
					is_face_visible = true;
				}
				break;
			}
		case CullMode::kNoCulling:
			{
				is_face_visible = true;
				break;
			}
		default:
			{
				break;
			}
		}

		if (!is_face_visible)
		{
			return false;
		}

		Vector3 edge1{ triangle.v1 - triangle.v0 };
		Vector3 edge2{ triangle.v2 - triangle.v0 };

		Vector3 h{ Vector3::Cross(ray.direction, edge2) }; // h is perpendicular to edge2 and the ray direction
		float det{ Vector3::Dot(edge1, h) };
		float inverse_det{ 1.f / det };

		Vector3 s{ ray.origin - triangle.v0 };
		float u{ Vector3::Dot(s, h) * inverse_det };

		if (u < 0.f || u > 1.f)
		{
			return false;
		}

		Vector3 q{ Vector3::Cross(s, edge1) };
		float v{ Vector3::Dot(ray.direction, q) * inverse_det };

		if (v < 0.f || u + v > 1.f)
		{
			return false;
		}

		float t{ Vector3::Dot(edge2, q) * inverse_det };
		if (t > ray.min && t < ray.max && ignore_hit_record == false && is_face_visible)
		{
			hit_record.t = t;
			hit_record.ray = ray;
			hit_record.barycentric_coordinates = Vector2{ u, v };
			return true;
		}
		
		return false;

	}

	[[maybe_unused]]
	static bool HitTestAABB(const AABB& aabb, const Ray& ray)
	{
		float t0x{ (aabb.min.x - ray.origin.x) / ray.direction.x };
		float t1x{ (aabb.max.x - ray.origin.x) / ray.direction.x };

		float t_min = std::min({ t0x, t1x });
		float t_max = std::max({ t0x, t1x });

		float t0y{ (aabb.min.y - ray.origin.y) / ray.direction.y };
		float t1y{ (aabb.max.y - ray.origin.y) / ray.direction.y };

		t_min = std::max({ t_min, std::min({t0y, t1y}) });
		t_max = std::min({ t_max, std::max({t0y, t1y}) });

		float t0z{ (aabb.min.z - ray.origin.z) / ray.direction.z };
		float t1z{ (aabb.max.z - ray.origin.z) / ray.direction.z };

		t_min = std::max({ t_min, std::min({t0z, t1z}) });
		t_max = std::min({ t_max, std::max({t0z, t1z}) });

		if (t_min <= t_max && t_max > 0.0f && t_min <= ray.max && t_max >= ray.min)
		{
			return true;
		}
		return false;
	}
}
#endif //INTERSECTIONS_HEADER