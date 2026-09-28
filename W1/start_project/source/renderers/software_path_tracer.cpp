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

	// DEMO CODE - TODO: remove!

	const SurfaceInfo& surface_info = context_->surface_info;
	const float aspectRatio{ static_cast<float>( surface_info.width ) / static_cast<float>( surface_info.height )};

	const double fovAngleRadians = static_cast<double>(context_->scene_manager->GetActiveScene()->camera.GetFovAngle()) * std::numbers::pi / 180.0;
	const float fov{ static_cast<float>(tan(fovAngleRadians / 2.0)) };

	for (uint32_t py = 0; py < surface_info.height; ++py)
	{
		for (uint32_t px = 0; px < surface_info.width; ++px)
		{
			float x{ (2 * ((px + 0.5f) / surface_info.width) - 1) * aspectRatio * fov } ;
			float y{ (1 - 2 * ((py + 0.5f) / surface_info.height)) * fov };

			Vector3 ray_direction{ x,y,1.f };
			ray_direction.Normalize();
			Ray viewRay{ context_->scene_manager->GetActiveScene()->camera.GetPosition(), ray_direction };
			ColorRgba final_color{ 0.f, 0.f, 0.f};

			RayHitRecord closest_hit_record{};
			ShadingInput shading_input{};

			if (SceneClosestHitTest(viewRay, closest_hit_record, false))
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
					switch (closest_hit_record.object_index)
					{
					case 0:
						final_color = (ColorRgba{ 0.f, 0.f, 0.f });
						break;
					case 1:
						final_color = (ColorRgba{1.f, 0.f, 0.f });
						break;
					case 2:
						final_color = (ColorRgba{ 0.f, 1.f, 0.f });
						break;
					case 3:
						final_color = (ColorRgba{ 1.f, 1.f, 0.f });
						break;
					case 4:
						final_color = (ColorRgba{ 0.f, 0.f, 1.f });
						break;
					}
					
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

		RayHitRecord temp_hit_record{};
		bool hit_primitive{ false };

		switch (primitive->type)
		{
		case PrimitiveType::kSphere:
			if (HitTestSphere(static_cast<const Sphere&>(*primitive), ray, temp_hit_record, ignore_record))
			{
				hit_primitive = true;
			}
			break;

		case PrimitiveType::kPlane:
			if (HitTestPlane(static_cast<const Plane&>(*primitive), ray, temp_hit_record, ignore_record))
			{
				hit_primitive = true;
			}
			break;

		default:
			break;
		}
		
		if (hit_primitive == true && temp_hit_record.t < closest_ray_length)
		{
			closest_ray_length = temp_hit_record.t;
			closest_hit = temp_hit_record;
			closest_hit.object_index = index;
			//closest_hit.ray = ray;
			hit_something = true;
		}
	}

	return hit_something;
	
}

ShadingInput SoftwarePathTracer::ConstructShadingInput(const RayHitRecord& hit) const
{
	ShadingInput shading_input{};
	shading_input.world_position = hit.ray.origin + hit.t * hit.ray.direction;
	
	const SceneObject& scene_object = context_->scene_manager->GetActiveScene()->objects[hit.object_index];
	Primitive* primitive{ context_->scene_manager->GetActiveScene()->primitives_factory.Get(scene_object.primitive_index) };

	switch (primitive->type)
	{
	case PrimitiveType::kSphere:
		{
			shading_input.world_normal = (shading_input.world_position - static_cast<Sphere&>(*primitive).origin).Normalized();
			break;
		}

	case PrimitiveType::kPlane:
		{
			shading_input.world_normal = static_cast<Plane&>(*primitive).normal;
			break;
		}
	
	default:
		break;
	}

	return shading_input;

}