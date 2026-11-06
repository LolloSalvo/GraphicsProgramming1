//==============================================================
//  Graphics Programming 1 (2026-2027)
//  Authors : Matthieu Delaere
//  Copyright (c) 2026 Matthieu Delaere. All rights reserved.
//==============================================================
#include <primitives.h>
using namespace gfx;

#include <cassert>

//--- Local struct and helper for Triangle Mesh
namespace
{
	struct TriangleInfo
	{
		AABB aabb;
		Vector3 centroid;
	};

	void BuildBVHNode(
		std::vector<BVHNode>& bvh_nodes,
		std::vector<uint32_t>& bvh_tri_indices,
		const std::vector<TriangleInfo>& triangles,
		uint32_t node_index
	)
	{
		const uint32_t first_tri = bvh_nodes[node_index].first_tri;
		const uint32_t tri_count = bvh_nodes[node_index].tri_count;

		if (tri_count <= 4)
		{
			return;
		}

		Vector3 extent = bvh_nodes[node_index].bounds.GetSize();

		uint8_t axis{};
		float midpoint{};

		if (extent.y > extent.x && extent.y >= extent.z)
		{
			axis = 1; // Y
			midpoint = bvh_nodes[node_index].bounds.GetCenter().y;
		}
		else if (extent.z > extent.x && extent.z > extent.y)
		{
			axis = 2; // Z
			midpoint = bvh_nodes[node_index].bounds.GetCenter().z;
		}
		else
		{
			axis = 0; // X
			midpoint = bvh_nodes[node_index].bounds.GetCenter().x;
		}

		uint32_t left = first_tri;
		uint32_t right = first_tri + tri_count;

		while (left < right)
		{
			if (triangles[bvh_tri_indices[left]].centroid[axis] < midpoint)
			{
				++left;
			}
			else
			{
				--right;
				std::swap(bvh_tri_indices[left], bvh_tri_indices[right]);
			}
		}

		if (left == first_tri || left == first_tri + tri_count)
		{
			return;
		}

		bvh_nodes[node_index].left_child = static_cast<uint32_t>(bvh_nodes.size());

		bvh_nodes.emplace_back();
		bvh_nodes.emplace_back();

		bvh_nodes[bvh_nodes[node_index].left_child].first_tri = first_tri;
		bvh_nodes[bvh_nodes[node_index].left_child].tri_count = left - first_tri;

		bvh_nodes[bvh_nodes[node_index].left_child + 1].first_tri = left;
		bvh_nodes[bvh_nodes[node_index].left_child + 1].tri_count = first_tri + tri_count - left;

		bvh_nodes[bvh_nodes[node_index].left_child].bounds = triangles[bvh_tri_indices[first_tri]].aabb;

		bvh_nodes[bvh_nodes[node_index].left_child + 1].bounds = triangles[bvh_tri_indices[left]].aabb;

		for (uint32_t i = first_tri; i < left; ++i)
		{
			bvh_nodes[bvh_nodes[node_index].left_child].bounds.ExpandToInclude(
				triangles[bvh_tri_indices[i]].aabb
			);
		}

		for (uint32_t i = left; i < first_tri + tri_count; ++i)
		{
			bvh_nodes[bvh_nodes[node_index].left_child + 1].bounds.ExpandToInclude(
				triangles[bvh_tri_indices[i]].aabb
			);
		}

		bvh_nodes[node_index].tri_count = 0;
	}
}

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

	if (indices.size() < 3)
	{
		bvh_tri_indices.clear();
		bvh_nodes.clear();
		return;
	}

	bvh_tri_indices.clear();

	std::vector<TriangleInfo> triangles(indices.size() / 3);
	for (uint32_t index{}; index < indices.size() / 3; ++index)
	{
		Vector3 v0{ vertices[indices[index * 3]].position };
		Vector3 v1{ vertices[indices[index * 3 + 1]].position };
		Vector3 v2{ vertices[indices[index * 3 + 2]].position };

		triangles[index].aabb.min = Vector3{
			std::min({v0.x, v1.x, v2.x}),
			std::min({v0.y, v1.y, v2.y}),
			std::min({v0.z, v1.z, v2.z})
		};
		triangles[index].aabb.max = Vector3{
			std::max({v0.x, v1.x, v2.x}),
			std::max({v0.y, v1.y, v2.y}),
			std::max({v0.z, v1.z, v2.z})
		};

		triangles[index].centroid = Vector3{
			(v0.x + v1.x + v2.x) / 3.f,
			(v0.y + v1.y + v2.y) / 3.f,
			(v0.z + v1.z + v2.z) / 3.f
		};

		bvh_tri_indices.push_back(index);
	}

	bvh_nodes.clear();
	bvh_nodes.emplace_back();

	bvh_nodes[0].first_tri = 0;
	bvh_nodes[0].tri_count = static_cast<uint32_t>(triangles.size());

	if (!triangles.empty())
	{
		bvh_nodes[0].bounds = triangles[0].aabb;
		for (uint32_t index{bvh_nodes[0].first_tri}; index < bvh_nodes[0].first_tri + bvh_nodes[0].tri_count; ++index)
		{
			bvh_nodes[0].bounds.ExpandToInclude(triangles[bvh_tri_indices[index]].aabb);
		}
	}

	for (uint32_t i = 0; i < bvh_nodes.size(); ++i)
	{
		BuildBVHNode(bvh_nodes, bvh_tri_indices, triangles, i);
	}

	if (!triangles.empty())
	{
		this->bounding_box->min = bvh_nodes[0].bounds.min;
		this->bounding_box->max = bvh_nodes[0].bounds.max;
	}
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