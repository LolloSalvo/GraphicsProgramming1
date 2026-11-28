//==============================================================
//  Graphics Programming 1 (2026-2027)
//  Authors : Matthieu Delaere
//  Copyright (c) 2026 Matthieu Delaere. All rights reserved.
//==============================================================
#include <software_rasterizer.h>
#include <scenes.h>
using namespace gfx;

// =============================================================================
// Construction / Destruction
// =============================================================================
SoftwareRasterizer::SoftwareRasterizer(Context* const context)
	: Renderer(context)
{}

SoftwareRasterizer::~SoftwareRasterizer() = default;

// =============================================================================
// Public Functions
// =============================================================================
void SoftwareRasterizer::Render()
{
	assert(context_ && "Context not available!");

	const SurfaceInfo& surface_info = context_->surface_info;

	const ColorRgba clear_color{ 0.19f, 0.21f, 0.26f };
	const uint32_t mapped_clear = SDL_MapRGB(
		surface_info.pixel_format_details, nullptr,
		static_cast<uint8_t>(clear_color.r * 255),
		static_cast<uint8_t>(clear_color.g * 255),
		static_cast<uint8_t>(clear_color.b * 255));
	std::fill_n(surface_info.pixel_buffer, surface_info.width * surface_info.height, mapped_clear);

	depth_buffer_.assign(surface_info.width * surface_info.height, kClearDepth);

	Scene* active_scene = context_->scene_manager->GetActiveScene();
	if (!active_scene)
	{
		return;
	}

	const float aspect_ratio{ static_cast<float>(surface_info.width) / static_cast<float>(surface_info.height) };
	const float fov_rad{ active_scene->camera.GetFovAngle() * (std::numbers::pi_v<float> / 180.f) };
	const float tan_half_fov{ std::tan(fov_rad * 0.5f) };

	const Matrix view_matrix{ active_scene->camera.GetView() };
	const Matrix projection_matrix{ active_scene->camera.GetProjection(tan_half_fov, aspect_ratio) };

	uint32_t object_id{ 0 };
	for (const auto& object : active_scene->objects)
	{
		const auto* mesh = active_scene->primitives_factory.GetAs<TriangleMesh>(object.primitive_index);
		if (!mesh)
		{
			++object_id;
			continue;
		}

		auto mesh_clone = mesh->CloneAs<TriangleMesh>();

		const Matrix world_matrix{ object.instance_transformation.value_or(Matrix::CreateIdentity()) };
		TransformMesh(mesh_clone.get(), world_matrix, view_matrix, projection_matrix);

		for (size_t i{ 0 }; i + 2 < mesh_clone->indices.size(); i += 3)
		{
			Vertex v0 = mesh_clone->vertices[mesh_clone->indices[i]];
			Vertex v1 = mesh_clone->vertices[mesh_clone->indices[i + 1]];
			Vertex v2 = mesh_clone->vertices[mesh_clone->indices[i + 2]];

			if (NotInFrustum(v0.position) || NotInFrustum(v1.position) || NotInFrustum(v2.position))
			{
				continue;
			}

			NdcToScreen(v0.position, surface_info.width, surface_info.height);
			NdcToScreen(v1.position, surface_info.width, surface_info.height);
			NdcToScreen(v2.position, surface_info.width, surface_info.height);

			RasterizeTriangle(v0, v1, v2, object_id, active_scene, mesh_clone->cull_mode);
		}

		++object_id;
	}
}

void SoftwareRasterizer::NdcToScreen(Vector4& position, uint32_t width, uint32_t height)
{
	float screen_width{ static_cast<float>(width) };
	float screen_height{ static_cast<float>(height) };

	position.x = ((position.x + 1.f) / 2.f) * screen_width;
	position.y = ((1.f - position.y) / 2.f) * screen_height;
}

void SoftwareRasterizer::RasterizeTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2,
	uint32_t object_id, [[maybe_unused]] const Scene* scene, CullMode cull_mode)
{
	const SurfaceInfo& surface_info = context_->surface_info;

	const float max_width{ static_cast<float>(surface_info.width - 1) };
	const float max_height{ static_cast<float>(surface_info.height - 1) };

	const float min_x_f{ std::clamp(std::min({ v0.position.x, v1.position.x, v2.position.x }), 0.f, max_width) };
	const float max_x_f{ std::clamp(std::max({ v0.position.x, v1.position.x, v2.position.x }), 0.f, max_width) };
	const float min_y_f{ std::clamp(std::min({ v0.position.y, v1.position.y, v2.position.y }), 0.f, max_height) };
	const float max_y_f{ std::clamp(std::max({ v0.position.y, v1.position.y, v2.position.y }), 0.f, max_height) };

	const uint32_t min_x{ static_cast<uint32_t>(min_x_f) };
	const uint32_t max_x{ static_cast<uint32_t>(max_x_f) };
	const uint32_t min_y{ static_cast<uint32_t>(min_y_f) };
	const uint32_t max_y{ static_cast<uint32_t>(max_y_f) };

	if (context_->debug_params.show_bounding_boxes)
	{
		const uint8_t r{ static_cast<uint8_t>((object_id * 97 + 60) % 256) };
		const uint8_t g{ static_cast<uint8_t>((object_id * 53 + 120) % 256) };
		const uint8_t b{ static_cast<uint8_t>((object_id * 151 + 180) % 256) };
		const uint32_t box_color{ SDL_MapRGB(surface_info.pixel_format_details, nullptr, r, g, b) };

		for (uint32_t py{ min_y }; py <= max_y; ++py)
		{
			for (uint32_t px{ min_x }; px <= max_x; ++px)
			{
				surface_info.pixel_buffer[px + (py * surface_info.width)] = box_color;
			}
		}
		return;
	}

	for (uint32_t py{ min_y }; py <= max_y; ++py)
	{
		for (uint32_t px{ min_x }; px <= max_x; ++px)
		{
			const Vector2 pixel{ static_cast<float>(px) + 0.5f, static_cast<float>(py) + 0.5f };
			RasterHitRecord hit_record{};

			if (!PointInTriangle(v0, v1, v2, pixel, cull_mode, hit_record))
			{
				continue;
			}

			const uint32_t pixel_index{ px + (py * surface_info.width) };

			if (hit_record.barycentric_coordinates.has_value())
			{
				const float depth{ InterpolateAttribute<float>(
					{ v0.position.z, v1.position.z, v2.position.z },
					hit_record.barycentric_coordinates.value()) };

				if (depth <= depth_buffer_[pixel_index])
				{
					continue;
				}

				depth_buffer_[pixel_index] = depth;
			}

			ColorRgba final_color{ 1.f, 1.f, 1.f };
			if (hit_record.barycentric_coordinates.has_value() && v0.color.has_value())
			{
				final_color = InterpolateAttribute<ColorRgba>(
					{ v0.color.value(), v1.color.value(), v2.color.value() },
					hit_record.barycentric_coordinates.value());
			}

			final_color.MaxToOne();
			surface_info.pixel_buffer[pixel_index] = SDL_MapRGB(
				surface_info.pixel_format_details, nullptr,
				static_cast<uint8_t>(final_color.r * 255),
				static_cast<uint8_t>(final_color.g * 255),
				static_cast<uint8_t>(final_color.b * 255));
		}
	}
}

bool SoftwareRasterizer::PointInTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2,
	const Vector2& pixel, CullMode cull_mode, RasterHitRecord& hit_record)
{
	const Vector2 p0{ v0.position.x, v0.position.y };
	const Vector2 p1{ v1.position.x, v1.position.y };
	const Vector2 p2{ v2.position.x, v2.position.y };

	const float area{ Vector2::Cross(p1 - p0, p2 - p0) };

	if (cull_mode == CullMode::kBackFaceCulling && area <= 0.f) return false;
	if (cull_mode == CullMode::kFrontFaceCulling && area >= 0.f) return false;
	if (area == 0.f) return false;

	const float c0{ Vector2::Cross(p1 - p0, pixel - p0) };
	const float c1{ Vector2::Cross(p2 - p1, pixel - p1) };
	const float c2{ Vector2::Cross(p0 - p2, pixel - p2) };

	const bool inside = (area > 0.f)
		? (c0 >= 0.f && c1 >= 0.f && c2 >= 0.f)
		: (c0 <= 0.f && c1 <= 0.f && c2 <= 0.f);

	if (!inside)
	{
		return false;
	}

	const float w1{ c2 / area };
	const float w2{ c0 / area };
	hit_record.barycentric_coordinates = Vector2{ w1, w2 };

	return true;
}


void SoftwareRasterizer::TransformMesh(TriangleMesh* mesh, const Matrix& world_matrix,
	const Matrix& view_matrix, const Matrix& projection_matrix)
{
	const Matrix wvp{ world_matrix * view_matrix * projection_matrix };

	for (Vertex& vertex : mesh->vertices)
	{
		vertex.position = wvp.TransformPoint(vertex.position);

		vertex.position.x /= vertex.position.w;
		vertex.position.y /= vertex.position.w;
		vertex.position.z /= vertex.position.w;
	}
}

bool SoftwareRasterizer::NotInFrustum(const Vector4& ndc_position)
{
	return ndc_position.x < -1.f || ndc_position.x > 1.f
		|| ndc_position.y < -1.f || ndc_position.y > 1.f
		|| ndc_position.z < 0.f || ndc_position.z > 1.f;
}