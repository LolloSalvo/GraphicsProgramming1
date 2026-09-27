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
	for (uint32_t py = 0; py < surface_info.height; ++py)
	{
		for (uint32_t px = 0; px < surface_info.width; ++px)
		{
			float x{ (2 * ((px + 0.5f) / surface_info.width) - 1) * aspectRatio } ;
			float y{ 1 - 2 * ((py + 0.5f) / surface_info.height) };

			Vector3 ray_direction{ x,y,1.f };
			ray_direction.Normalize();
			Ray viewRay{ Vector3{0.f, 0.f, 0.f}, ray_direction };
			ColorRgba final_color{ 0.f, 0.f, 0.f};

			RayHitRecord closest_hit_record{};
			const Sphere testSphere{ {0.f, 0.f, 100.f}, 50.f };
			if (bool didHit{ HitTestSphere(testSphere, viewRay, closest_hit_record) })
			{
				ShadingInput shading_input{};
				shading_input.world_position = viewRay.origin + closest_hit_record.t * viewRay.direction;
				shading_input.world_normal = (shading_input.world_position - testSphere.origin).Normalized();

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
					final_color = (ColorRgba{ 1.f, 0.f, 0.f });
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