//==============================================================
//  Graphics Programming 1 (2026-2027)
//  Authors : Matthieu Delaere
//  Copyright (c) 2026 Matthieu Delaere. All rights reserved.
//==============================================================
#include <primitives.h>
using namespace gfx;

//--- Primitives AABB Updates ---
void Sphere::UpdateAABB()
{
	if (!this->bounding_box.has_value())
	{
		this->bounding_box.emplace();
	}

	this->bounding_box->min = Vector3{ this->origin.x - this->radius, this->origin.y - this->radius, this->origin.z - this->radius };
	this->bounding_box->max = Vector3{ this->origin.x + this->radius, this->origin.y + this->radius, this->origin.z + this->radius };
}

void Triangle::UpdateAABB()
{
	if (!this->bounding_box.has_value())
	{
		this->bounding_box.emplace();
	}

	const float min_x{ std::min({this->v0.x, this->v1.x, this->v2.x}) };
	const float min_y{ std::min({this->v0.y, this->v1.y, this->v2.y}) };
	const float min_z{ std::min({this->v0.z, this->v1.z, this->v2.z}) };
	
	const float max_x{ std::max({this->v0.x, this->v1.x, this->v2.x}) };
	const float max_y{ std::max({this->v0.y, this->v1.y, this->v2.y}) };
	const float max_z{ std::max({this->v0.z, this->v1.z, this->v2.z}) };

	this->bounding_box->min = Vector3{ min_x, min_y, min_z };
	this->bounding_box->max = Vector3{ max_x, max_y, max_z };

}

void TriangleMesh::UpdateAABB()
{
	if (!this->bounding_box.has_value())
	{
		this->bounding_box.emplace();
	}

	Vector3 min_extents{
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max()
	};

	Vector3 max_extents{
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest()
	};

	for (const Vertex& vertex : this->vertices)
	{
		min_extents.x = std::min({min_extents.x, vertex.position.x});
		max_extents.x = std::max({max_extents.x, vertex.position.x});

		min_extents.y = std::min({min_extents.y, vertex.position.y});
		max_extents.y = std::max({max_extents.y, vertex.position.y});
	
		min_extents.z = std::min({min_extents.z, vertex.position.z});
		max_extents.z = std::max({max_extents.z, vertex.position.z});
	}

	this->bounding_box->min = min_extents;
	this->bounding_box->max = max_extents;
}

//--- AABB ---
void AABB::ExpandToInclude(const Vector3& point)
{
	min = Vector3::Min(min, point);
	max = Vector3::Max(max, point);
}

void AABB::ExpandToInclude(const AABB& other)
{
	min = Vector3::Min(min, other.min);
	max = Vector3::Max(max, other.max);
}

bool AABB::Contains(const Vector3& point) const
{
	return point.x >= min.x && point.x <= max.x &&
		point.y >= min.y && point.y <= max.y &&
		point.z >= min.z && point.z <= max.z;
}

bool AABB::Overlaps(const AABB& other) const
{
	return min.x <= other.max.x && max.x >= other.min.x &&
		min.y <= other.max.y && max.y >= other.min.y &&
		min.z <= other.max.z && max.z >= other.min.z;
}

Vector3 AABB::GetCenter() const
{
	return { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
}

Vector3 AABB::GetSize() const
{
	return { max.x - min.x, max.y - min.y, max.z - min.z };
}

bool AABB::IsValid() const
{
	return min.x <= max.x && min.y <= max.y && min.z <= max.z;
}