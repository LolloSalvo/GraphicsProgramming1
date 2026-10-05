//==============================================================
//  Graphics Programming 1 (2026-2027)
//  Authors : Matthieu Delaere
//  Copyright (c) 2026 Matthieu Delaere. All rights reserved.
//==============================================================
#include <software_path_tracer.h>
#include <scenes.h>
#include <intersections.h>
using namespace gfx;

// =============================================================================
// Construction / Destruction
// =============================================================================
SoftwarePathTracer::SoftwarePathTracer(Context* const context)
	: Renderer(context)
{}

SoftwarePathTracer::~SoftwarePathTracer() = default;

// =============================================================================
// Public Functions
// =============================================================================
void SoftwarePathTracer::Render()
{
	assert(context_ && "Context not available!");

	const SurfaceInfo& surface_info = context_->surface_info;
	const float aspectRatio{ static_cast<float>( surface_info.width ) / static_cast<float>( surface_info.height )};

	const double fovAngleRadians = static_cast<double>(context_->scene_manager->GetActiveScene()->camera.GetFovAngle()) * std::numbers::pi / 180.0;
	const float fov{ static_cast<float>(tan(fovAngleRadians / 2.0)) };

	Matrix view_matrix{ context_->scene_manager->GetActiveScene()->camera.GetView() };
	Matrix world_matrix{ view_matrix.GetInverse() };

	for (uint32_t py = 0; py < surface_info.height; ++py)
	{
		for (uint32_t px = 0; px < surface_info.width; ++px)
		{

			float x{ (2 * ((px + 0.5f) / surface_info.width) - 1) * aspectRatio * fov } ;
			float y{ (1 - 2 * ((py + 0.5f) / surface_info.height)) * fov };

			Vector3 ray_direction_camera_space{ x,y,1.f };

			Vector3 world_ray_direction{ world_matrix.TransformVector(ray_direction_camera_space) };
			world_ray_direction.Normalize();
			 
			Ray world_ray{ context_->scene_manager->GetActiveScene()->camera.GetPosition(), world_ray_direction };

			ColorRgba final_color{ 0.f, 0.f, 0.f};

			RayHitRecord closest_hit_record{};
			ShadingInput shading_input{};

			if (SceneClosestHitTest(world_ray, closest_hit_record, false))
			{
				shading_input = ConstructShadingInput(closest_hit_record);

				DebugParams debug_params{ context_->debug_params };
				VisualizationMode vis_mode{ debug_params.visualization_mode };
				
				if (debug_params.visualization_mode == VisualizationMode::kDepth)
				{
					const float max_depth{ 100.f };
					const float scaled_t{ 1.f -
						std::clamp(closest_hit_record.t / max_depth, 0.f, 1.f) };
					final_color = { scaled_t, scaled_t, scaled_t };
				}
				else if (vis_mode == VisualizationMode::kNormals)
				{
					const Vector3& n = shading_input.world_normal;
					final_color = {
						(n.x + 1.f) * 0.5f,
						(n.y + 1.f) * 0.5f,
						(n.z + 1.f) * 0.5f };
				}
				else if (vis_mode == VisualizationMode::kNone)
				{
					const uint32_t idx = closest_hit_record.object_index;
					final_color = {
						static_cast<float>(idx & 1),
						static_cast<float>((idx >> 1) & 1),
						static_cast<float>((idx >> 2) & 1)
					};
				}

			}


			//// Calculate a gradient value based on pixel screen coordinates.
			//float gradient = px / static_cast<float>(surface_info.width);
			//gradient += py / static_cast<float>(surface_info.height);
			//gradient /= 2.0f;

			//// Convert gradient value to color.
			//ColorRgba final_color = { gradient, gradient, gradient };
			//final_color.MaxToOne();

			// Write to surface
			surface_info.pixel_buffer[px + (py * surface_info.width)] = SDL_MapRGB(
				surface_info.pixel_format_details, nullptr,
				static_cast<uint8_t>(final_color.r * 255),
				static_cast<uint8_t>(final_color.g * 255),
				static_cast<uint8_t>(final_color.b * 255));
		}
	}
}

bool SoftwarePathTracer::SceneClosestHitTest(const Ray& ray, RayHitRecord& closest_hit, bool ignore_record) const
{
	float closest_ray_length{ ray.max };
	bool hit_something{ false };

	if (ignore_record)
	{
		return false;
	}

	Scene* active_scene{ context_->scene_manager->GetActiveScene() };
	if (active_scene == nullptr)
	{
		return false;
	}


	for (uint32_t index{}; index < active_scene->objects.size(); index++)
	{
		const SceneObject& scene_object{ active_scene->objects.at(index) };
		Primitive* primitive{ active_scene->primitives_factory.Get(scene_object.primitive_index) };

		Ray ray_model_space{ ray };
		if (scene_object.instance_transformation.has_value())
		{
			ray_model_space = scene_object.instance_transformation->TransformRay(ray);
		}


		RayHitRecord temp_hit_record{};
		bool hit_primitive{ false };

		switch (primitive->type)
		{
		case PrimitiveType::kSphere:
			if (HitTestSphere(static_cast<const Sphere&>(*primitive), ray_model_space, temp_hit_record, ignore_record))
			{
				hit_primitive = true;
			}
			break;

		case PrimitiveType::kPlane:
			if (HitTestPlane(static_cast<const Plane&>(*primitive), ray_model_space, temp_hit_record, ignore_record))
			{
				hit_primitive = true;
			}
			break;

		case PrimitiveType::kTriangle:
			if (HitTestTriangle(static_cast<const Triangle&>(*primitive), ray_model_space, temp_hit_record, ignore_record))
			{
				hit_primitive = true;
			}
			break;
		case PrimitiveType::kTriangleMesh:
		{
			const TriangleMesh& mesh = static_cast<const TriangleMesh&>(*primitive);

			for (int i{}; i < mesh.indices.size(); i += 3)
			{
				uint32_t idx0 = mesh.indices[i];
				uint32_t idx1 = mesh.indices[i + 1];
				uint32_t idx2 = mesh.indices[i + 2];

				Triangle triangle{};
				triangle.v0 = mesh.vertices[idx0].position;
				triangle.v1 = mesh.vertices[idx1].position;
				triangle.v2 = mesh.vertices[idx2].position;
				triangle.cull_mode = mesh.cull_mode;

				Vector3 edge1 = triangle.v1 - triangle.v0;
				Vector3 edge2 = triangle.v2 - triangle.v0;
				triangle.normal = Vector3::Cross(edge1, edge2).Normalized();

				if (HitTestTriangle(triangle, ray_model_space, temp_hit_record, ignore_record))
				{
					hit_primitive = true;

					if (!ignore_record)
					{
						temp_hit_record.vertex_indices = std::array<uint32_t, 3>{ idx0, idx1, idx2 };
					}

					if (ignore_record) break;
				}
			}
			break;
		}

		default:
			break;
		}
		
		if (hit_primitive == true && temp_hit_record.t < closest_ray_length)
		{
			closest_ray_length = temp_hit_record.t;

			closest_hit = temp_hit_record;
			closest_hit.object_index = index;

			closest_hit.ray = ray_model_space;

			hit_something = true;
		}
	}

	return hit_something;
	
}

ShadingInput SoftwarePathTracer::ConstructShadingInput(const RayHitRecord& hit) const
{
	ShadingInput shading_input{};

	Vector3 local_position = hit.ray.origin + hit.t * hit.ray.direction;
	Vector3 local_normal{};

	const SceneObject& scene_object = context_->scene_manager->GetActiveScene()->objects[hit.object_index];
	Primitive* primitive{ context_->scene_manager->GetActiveScene()->primitives_factory.Get(scene_object.primitive_index) };

	switch (primitive->type)
	{
		case PrimitiveType::kSphere:
		{
			local_normal = (local_position - static_cast<Sphere&>(*primitive).origin).Normalized();
			break;
		}
		case PrimitiveType::kPlane:
		{
			local_normal = static_cast<Plane&>(*primitive).normal;
			break;
		}
		case PrimitiveType::kTriangle:
		{
			local_normal = static_cast<Triangle&>(*primitive).normal;
			break;
		}
		case PrimitiveType::kTriangleMesh:
		{
			const TriangleMesh& mesh = static_cast<const TriangleMesh&>(*primitive);

			std::array<uint32_t, 3> indices = hit.vertex_indices.value();

			float u = hit.barycentric_coordinates.value().x;
			float v = hit.barycentric_coordinates.value().y;
			float w = 1.0f - u - v;

			const auto& norm0 = mesh.vertices[indices[0]].normal;
			const auto& norm1 = mesh.vertices[indices[1]].normal;
			const auto& norm2 = mesh.vertices[indices[2]].normal;

			if (norm0.has_value() && norm1.has_value() && norm2.has_value())
			{
				local_normal = (norm0.value() * w + norm1.value() * u + norm2.value() * v).Normalized();
			}
			else
			{
				Vector3 v0 = mesh.vertices[indices[0]].position;
				Vector3 v1 = mesh.vertices[indices[1]].position;
				Vector3 v2 = mesh.vertices[indices[2]].position;
				local_normal = Vector3::Cross(v1 - v0, v2 - v0).Normalized();
			}
			break;
		}
		default:
			break;
	}

	if (scene_object.instance_transformation.has_value())
	{
		shading_input.world_position = scene_object.instance_transformation->TransformPoint(local_position);
		shading_input.world_normal = scene_object.instance_transformation->TransformVector(local_normal).Normalized();
	}
	else
	{
		shading_input.world_position = local_position;
		shading_input.world_normal = local_normal;
	}

	return shading_input;
}