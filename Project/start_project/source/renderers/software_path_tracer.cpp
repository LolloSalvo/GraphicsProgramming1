//==============================================================
//  Graphics Programming 1 (2026-2027)
//  Authors : Matthieu Delaere
//  Copyright (c) 2026 Matthieu Delaere. All rights reserved.
//==============================================================
#include <software_path_tracer.h>
#include <scenes.h>
#include <intersections.h>

#include <samplers.h>

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

	int surface_height{ static_cast<int>(surface_info.height) };
	int surface_width{ static_cast<int>(surface_info.width) };

	Scene* active_scene{ context_->scene_manager->GetActiveScene() };
	DebugParams debug_params{ context_->debug_params };
	VisualizationMode current_vis_mode{ debug_params.visualization_mode };
	const SamplingStrategy current_sampling{ debug_params.sampling_strategy };
	const ToneMapOperator current_tone_map{ debug_params.tone_map_operator };

	uint32_t total_pixels{ static_cast<uint32_t>(surface_width * surface_height) };

	if (active_scene->scene_changed || accumulation_buffer_.size() != total_pixels
		|| current_vis_mode != last_vis_mode_
		|| current_sampling != last_sampling_strategy_
		|| current_tone_map != last_tone_map_operator_)
	{
		context_->debug_params.accumulated_samples = 0;
		accumulation_buffer_.assign(total_pixels, ColorRgba::Black());
		last_vis_mode_ = current_vis_mode;
		last_sampling_strategy_ = current_sampling;
		last_tone_map_operator_ = current_tone_map;
	}

	if (context_->debug_params.accumulated_samples >= context_->debug_params.max_samples)
	{
		return;
	}

	const bool is_debug_view{ current_vis_mode != VisualizationMode::kNone
		&& current_vis_mode != VisualizationMode::kDirectOnly
		&& current_vis_mode != VisualizationMode::kBounceCount };

#pragma omp parallel for schedule(dynamic, 16)
	for (int py{}; py < surface_height; ++py)
	{
		for (int px{}; px < surface_width; ++px)
		{
			uint32_t pixel_index{ px + (py * surface_info.width) };
			uint32_t hash_state{ LowBias32(pixel_index ^ (context_->debug_params.accumulated_samples * 2654435761u)) };

			const Vector2 jitter{ GenerateSubpixelJitter(pixel_index, hash_state) };

			float x{ (2 * ((static_cast<float>(px) + jitter.x) / surface_info.width) - 1) * aspectRatio * fov } ;
			float y{ (1 - 2 * ((static_cast<float>(py) + jitter.y) / surface_info.height)) * fov };

			Vector3 ray_direction_camera_space{ x,y,1.f };

			Vector3 world_ray_direction{ world_matrix.TransformVector(ray_direction_camera_space) };
			world_ray_direction.Normalize();
			 
			Ray world_ray{ active_scene->camera.GetPosition(), world_ray_direction };

			ColorRgba final_color{ ColorRgba::Black() };

			if (is_debug_view)
			{
				RayHitRecord closest_hit_record{};
				if (SceneClosestHitTest(world_ray, closest_hit_record, false))
				{
					ShadingInput shading_input = ConstructShadingInput(closest_hit_record);
					shading_input.view_direction = -world_ray_direction;

					const SceneObject& obj{ active_scene->objects.at(closest_hit_record.object_index) };
					const Material* material{ active_scene->materials_factory.Get(obj.material_index) };
					const SurfaceProperties surface{ material->GetSurfaceProperties(shading_input, active_scene->textures_factory) };

					shading_input.world_normal = surface.shading_normal;

					if (current_vis_mode == VisualizationMode::kDepth)
					{
						const float max_depth{ 100.f };
						const float scaled_t{ 1.f - std::clamp(closest_hit_record.t / max_depth, 0.f, 1.f) };
						final_color = { scaled_t, scaled_t, scaled_t };
					}
					else if (current_vis_mode == VisualizationMode::kNormals)
					{
						final_color = {
							(shading_input.world_normal.x + 1.f) * 0.5f,
							(shading_input.world_normal.y + 1.f) * 0.5f,
							(shading_input.world_normal.z + 1.f) * 0.5f };
					}
					else if (current_vis_mode == VisualizationMode::kAlbedo)
					{
						final_color = surface.albedo;
					}
					else if (current_vis_mode == VisualizationMode::kMetallicRoughness)
					{
						final_color = { 0.f, surface.roughness, surface.metallic };
					}
					else if (current_vis_mode == VisualizationMode::kUV)
					{
						if (shading_input.texture_coordinates.has_value())
						{
							final_color = {
								shading_input.texture_coordinates.value().x,
								shading_input.texture_coordinates.value().y,
								0.f };
						}
						else
						{
							final_color = { 0.f, 0.f, 0.f };
						}
					}
				}
			}
			else
			{
				ColorRgba sample_color{ TracePath(world_ray, active_scene, hash_state) };

				if (!std::isfinite(sample_color.r) || !std::isfinite(sample_color.g) || !std::isfinite(sample_color.b))
				{
					sample_color = ColorRgba::Black();
				}

				const float inv_frames{ 1.f / static_cast<float>(context_->debug_params.accumulated_samples + 1) };
				accumulation_buffer_[pixel_index] += sample_color;
				final_color = accumulation_buffer_[pixel_index] * inv_frames;

				if (current_vis_mode != VisualizationMode::kBounceCount)
				{
					final_color = final_color.Tonemap(debug_params.tone_map_operator);
					final_color = final_color.ToSrgb();
				}
			}
			
			final_color.Clamp();


			// Write to surface
			surface_info.pixel_buffer[px + (py * surface_info.width)] = SDL_MapRGB(
				surface_info.pixel_format_details, nullptr,
				static_cast<uint8_t>(final_color.r * 255),
				static_cast<uint8_t>(final_color.g * 255),
				static_cast<uint8_t>(final_color.b * 255));
		}
	}

	if (!is_debug_view)
	{
		++context_->debug_params.accumulated_samples;
	}
}

bool SoftwarePathTracer::SceneClosestHitTest(const Ray& ray, RayHitRecord& closest_hit, bool ignore_record, bool reverse_culling) const
{
	float closest_ray_length{ ray.max };
	bool hit_something{ false };
	const bool culling_flag{ ignore_record || reverse_culling };

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

		if (primitive->bounding_box.has_value() && primitive->type != PrimitiveType::kPlane)
		{
			if (!HitTestAABB(primitive->bounding_box.value(), ray_model_space))
			{
				continue;
			}
		}

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
		{
			Triangle triangle{ static_cast<const Triangle&>(*primitive) };
			if (culling_flag)
			{
				if (triangle.cull_mode == CullMode::kBackFaceCulling)
				{
					triangle.cull_mode = CullMode::kFrontFaceCulling;
				}
				else if (triangle.cull_mode == CullMode::kFrontFaceCulling)
				{
					triangle.cull_mode = CullMode::kBackFaceCulling;
				}
			}

			if (HitTestTriangle(triangle, ray_model_space, temp_hit_record, ignore_record))
			{
				hit_primitive = true;
			}
			break;
		}
		case PrimitiveType::kTriangleMesh:
		{
			const TriangleMesh& mesh = static_cast<const TriangleMesh&>(*primitive);

			if (mesh.bvh_nodes.empty())
			{
				break;
			}

			std::array<uint32_t, 64> stack{};
			uint32_t stack_size = 1;
			stack[0] = 0;

			while (stack_size > 0)
			{
				const uint32_t node_index = stack[--stack_size];
				const BVHNode& node = mesh.bvh_nodes[node_index];

				if (!HitTestAABB(node.bounds, ray_model_space))
				{
					continue;
				}

				if (node.IsLeaf())
				{
					for (uint32_t j{ node.first_tri}; j < node.first_tri + node.tri_count; ++j)
					{
						const uint32_t triangle_index = mesh.bvh_tri_indices[j];

						const uint32_t idx0 = mesh.indices[triangle_index * 3];
						const uint32_t idx1 = mesh.indices[triangle_index * 3 + 1];
						const uint32_t idx2 = mesh.indices[triangle_index * 3 + 2];

						Triangle triangle{};
						triangle.v0 = mesh.vertices[idx0].position;
						triangle.v1 = mesh.vertices[idx1].position;
						triangle.v2 = mesh.vertices[idx2].position;
						triangle.cull_mode = mesh.cull_mode;
						if (culling_flag)
						{
							if (triangle.cull_mode == CullMode::kBackFaceCulling)
							{
								triangle.cull_mode = CullMode::kFrontFaceCulling;
							}
							else if (triangle.cull_mode == CullMode::kFrontFaceCulling)
							{
								triangle.cull_mode = CullMode::kBackFaceCulling;
							}
						}

						const Vector3 edge1 = triangle.v1 - triangle.v0;
						const Vector3 edge2 = triangle.v2 - triangle.v0;
						triangle.normal = Vector3::Cross(edge1, edge2).Normalized();

						if (HitTestTriangle(triangle, ray_model_space, temp_hit_record, ignore_record))
						{
							hit_primitive = true;

							if (!ignore_record)
							{
								temp_hit_record.vertex_indices =
									std::array<uint32_t, 3>{ idx0, idx1, idx2 };

								ray_model_space.max = temp_hit_record.t;
							}
						}
					}
				}
				else
				{
					assert(stack_size + 2 <= stack.size());

					stack[stack_size++] = node.left_child;
					stack[stack_size++] = node.left_child + 1;
				}
			}

			break;
		}

		default:
			break;
		}
		
		if (hit_primitive == true)
		{
			if (ignore_record)
			{
				return true;
			}

			if (temp_hit_record.t < closest_ray_length)
			{
				closest_ray_length = temp_hit_record.t;

				closest_hit = temp_hit_record;
				closest_hit.object_index = index;

				closest_hit.ray = ray_model_space;
				
				hit_something = true;
			}

			//if (primitive->type == PrimitiveType::kTriangleMesh)
			//{
			//	closest_hit.object_index = 8;
			//}
			//else
			//{
			//	closest_hit.object_index = index;
			//}


		}

	}

	return hit_something;
	
}

ShadingInput SoftwarePathTracer::ConstructShadingInput(const RayHitRecord& hit) const
{
	ShadingInput shading_input{};

	Vector3 local_position = hit.ray.origin + hit.t * hit.ray.direction;
	Vector3 local_normal{};
	Vector3 local_shadow_origin{};

	Vector3 local_tangent{};
	Vector3 local_bitangent{};

	const SceneObject& scene_object = context_->scene_manager->GetActiveScene()->objects[hit.object_index];
	Primitive* primitive{ context_->scene_manager->GetActiveScene()->primitives_factory.Get(scene_object.primitive_index) };

	switch (primitive->type)
	{
		case PrimitiveType::kSphere:
		{
			local_normal = (local_position - static_cast<Sphere&>(*primitive).origin).Normalized();
			local_shadow_origin = local_position + local_normal * 0.001f;
			break;
		}
		case PrimitiveType::kPlane:
		{
			local_normal = static_cast<Plane&>(*primitive).normal;
			local_shadow_origin = local_position + local_normal * 0.001f;
			break;
		}
		case PrimitiveType::kTriangle:
		{
			local_normal = static_cast<Triangle&>(*primitive).normal;
			local_shadow_origin = local_position + local_normal * 0.001f;
			break;
		}
		case PrimitiveType::kTriangleMesh:
		{
			const TriangleMesh& mesh = static_cast<const TriangleMesh&>(*primitive);

			std::array<uint32_t, 3> indices = hit.vertex_indices.value();

			Vector3 v0 = mesh.vertices[indices[0]].position;
			Vector3 v1 = mesh.vertices[indices[1]].position;
			Vector3 v2 = mesh.vertices[indices[2]].position;

			float u = hit.barycentric_coordinates.value().x;
			float v = hit.barycentric_coordinates.value().y;
			float w = 1.0f - u - v;

			const auto& norm0 = mesh.vertices[indices[0]].normal;
			const auto& norm1 = mesh.vertices[indices[1]].normal;
			const auto& norm2 = mesh.vertices[indices[2]].normal;

			const auto& uv0 = mesh.vertices[indices[0]].texture_coordinates;
			const auto& uv1 = mesh.vertices[indices[1]].texture_coordinates;
			const auto& uv2 = mesh.vertices[indices[2]].texture_coordinates;

			const auto& tan0 = mesh.vertices[indices[0]].tangent;
			const auto& tan1 = mesh.vertices[indices[1]].tangent;
			const auto& tan2 = mesh.vertices[indices[2]].tangent;

			const auto& bitan0 = mesh.vertices[indices[0]].bitangent;
			const auto& bitan1 = mesh.vertices[indices[1]].bitangent;
			const auto& bitan2 = mesh.vertices[indices[2]].bitangent;

			if (tan0.has_value() && tan1.has_value() && tan2.has_value())
			{
				local_tangent = (tan0.value() * w + tan1.value() * u + tan2.value() * v).Normalized();
			}

			if (bitan0.has_value() && bitan1.has_value() && bitan2.has_value())
			{
				local_bitangent = (bitan0.value() * w + bitan1.value() * u + bitan2.value() * v).Normalized();
			}

			if (uv0.has_value() && uv1.has_value() && uv2.has_value())
			{
				shading_input.texture_coordinates = uv0.value() * w + uv1.value() * u + uv2.value() * v;
			}

			if (norm0.has_value() && norm1.has_value() && norm2.has_value())
			{
				local_normal = (norm0.value() * w + norm1.value() * u + norm2.value() * v).Normalized();

				Vector3 tmpu{ local_position - v0 };
				Vector3 tmpv{ local_position - v1 };
				Vector3 tmpw{ local_position - v2 };

				float dotu{ std::min(0.0f, Vector3::Dot(tmpu, norm0.value())) };
				float dotv{ std::min(0.0f, Vector3::Dot(tmpv, norm1.value())) };
				float dotw{ std::min(0.0f, Vector3::Dot(tmpw, norm2.value())) };

				Vector3 offset { norm0.value() * (w * dotu) + norm1.value() * (u * dotv) + norm2.value() * (v * dotw) };

				Vector3 face_normal{ Vector3::Cross(v1 - v0, v2 - v0).Normalized() };

				if (Vector3::Dot(face_normal, local_normal) < 0.0f)
				{
					face_normal = -face_normal;
				}

				local_shadow_origin = local_position + offset + face_normal * 0.001f;
				
			}
			else
			{
				local_normal = Vector3::Cross(v1 - v0, v2 - v0).Normalized();

				local_shadow_origin = local_position + local_normal * 0.001f;
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
		shading_input.shadow_origin = scene_object.instance_transformation->TransformPoint(local_shadow_origin);
		shading_input.world_tangent = scene_object.instance_transformation->TransformVector(local_tangent).Normalized();
		shading_input.world_bitangent = scene_object.instance_transformation->TransformVector(local_bitangent).Normalized();
	}
	else
	{
		shading_input.world_position = local_position;
		shading_input.world_normal = local_normal;
		shading_input.shadow_origin = local_shadow_origin;
		shading_input.world_tangent = local_tangent;
		shading_input.world_bitangent = local_bitangent;
	}

	return shading_input;
}

ColorRgba SoftwarePathTracer::DirectIllumination(const ShadingInput& shading_input, const SurfaceProperties& surface,
                                                 const Material* material, const Scene* scene, uint32_t& hash_state) const
{
	ColorRgba final_color{ ColorRgba::Black() };
	ShadingInput input{ shading_input };
	Vector3 light_dir{};
	for (Light* light : scene->lights_factory.GetAll())
	{
		if (light->type == LightType::kDirectional)
		{
			light_dir = -light->direction;
		}
		else
		{
			light_dir = light->origin - input.world_position;
		}

		const float light_distance{ light_dir.Normalize() };
		//light_dir.Normalize();

		const float cos_theta{ Vector3::Dot(input.world_normal, light_dir) };
		if (cos_theta <= 0.f)
		{
			continue;
		}

		const Ray shadow_ray{
			.origin = input.shadow_origin,
			.direction = light_dir,
			.min = 0.001f,
			.max = light->type == LightType::kDirectional
				? std::numeric_limits<float>::max() : light_distance
		};
		RayHitRecord shadow_hit{};

		const ColorRgba transmittance{ ShadowTransmittance(shadow_ray, scene, hash_state) };
		if (transmittance.Luminance() <= 0.f)
		{
			continue;
		}

		input.light_direction = light_dir;

		ColorRgba radiance = CalculateRadiance(light, input.world_position);

		ColorRgba brdf{ material->Shade(input, surface) };

		final_color += transmittance * radiance * brdf * cos_theta;

	}

	return final_color;
}

ColorRgba SoftwarePathTracer::CalculateRadiance(Light* light, const Vector3& world_pos) const
{
	switch (light->type)
	{
	case LightType::kPoint:
		{
			return ColorRgba{ light->color * (light->intensity / (light->origin - world_pos).SqrMagnitude()) };
		}
	case LightType::kDirectional:
		{
		return ColorRgba{ light->color * light->intensity };
		}
	default:
		return { 0.f, 0.f, 0.f };
	}
}

Vector2 SoftwarePathTracer::GenerateSubpixelJitter(uint32_t /*pixel_index*/, uint32_t& hash_state) const
{

	const float jx{ NextFloat(hash_state) };
	const float jy{ NextFloat(hash_state) };

	return { jx, jy };

}

ColorRgba SoftwarePathTracer::TracePath(const Ray& ray, const Scene* scene, uint32_t& hash_state, int max_bounces) const
{
	ColorRgba accumulated_color{ ColorRgba::Black() };
	ColorRgba throughput{ ColorRgba::White() };

	Ray current_ray{ ray };
	int bounce{ 0 };

	MediumState medium{};

	for (; bounce < max_bounces; ++bounce)
	{
		RayHitRecord hit{};
		if (!SceneClosestHitTest(current_ray, hit, false, medium.inside))
		{
			accumulated_color += throughput * EnvironmentRadiance(scene, current_ray.direction);
			break;
		}

		ShadingInput shading_input{ ConstructShadingInput(hit) };
		shading_input.view_direction = -current_ray.direction;

		const SceneObject& obj{ scene->objects.at(hit.object_index) };
		const Material* material{ scene->materials_factory.Get(obj.material_index) };
		SurfaceProperties surface{ material->GetSurfaceProperties(shading_input, scene->textures_factory) };

		if (IsAlphaSkipped(surface, hash_state))
		{
			current_ray.origin += current_ray.direction * (hit.t + 0.001f);
			--bounce;
			continue;
		}

		shading_input.world_normal = surface.shading_normal;

		surface.roughness = std::max(surface.roughness, 0.01f);

		if (medium.inside && medium.attenuation_distance < std::numeric_limits<float>::max())
		{
			const float exponent{ hit.t / medium.attenuation_distance };
			throughput *= ColorRgba(
				std::pow(medium.attenuation_color.r, exponent),
				std::pow(medium.attenuation_color.g, exponent),
				std::pow(medium.attenuation_color.b, exponent));
		}

		if (surface.transmission <= 0.f)
		{
			accumulated_color += throughput * DirectIllumination(shading_input, surface, material, scene, hash_state);
		}

		if (context_->debug_params.visualization_mode == VisualizationMode::kDirectOnly)
		{
			break;
		}

		if (bounce >= 3)
		{
			const float survival_probability{ std::clamp(throughput.Luminance(), 0.05f, 0.95f) };
			if (NextFloat(hash_state) > survival_probability)
			{
				break;
			}
			throughput *= (1.f / survival_probability);
		}

		BounceSample bounce_sample{};
		if (surface.transmission > 0.f)
		{
			bounce_sample = SampleReflectOrRefract(shading_input, surface, medium, hash_state);
		}
		else
		{
			HemisphereSample sample{};
			if (context_->debug_params.sampling_strategy == SamplingStrategy::kGGX)
			{
				sample = SampleSpecularOrDiffuse(shading_input, surface, hash_state);
			}
			else if (context_->debug_params.sampling_strategy == SamplingStrategy::kUniform)
			{
				sample = SampleUniformHemisphere(shading_input.world_normal, hash_state);
			}
			else
			{
				sample = SampleCosineHemisphere(shading_input.world_normal, hash_state);
			}

			const float n_dot_l{ Vector3::Dot(shading_input.world_normal, sample.direction) };
			if (n_dot_l <= 0.f || sample.pdf < 1e-6f)
			{
				break;
			}

			shading_input.light_direction = sample.direction;
			shading_input.is_indirect = true;

			bounce_sample.direction = sample.direction;
			bounce_sample.weight = material->Shade(shading_input, surface) * n_dot_l / sample.pdf;
		}

		throughput *= bounce_sample.weight;

		const bool same_side_as_normal{ Vector3::Dot(bounce_sample.direction, shading_input.world_normal) > 0.f };
		current_ray = {
			.origin = same_side_as_normal
				? shading_input.shadow_origin
				: shading_input.world_position - shading_input.world_normal * 0.001f,
			.direction = bounce_sample.direction,
			.min = 0.001f,
			.max = std::numeric_limits<float>::max()
		};
		
	}

	if (context_->debug_params.visualization_mode == VisualizationMode::kBounceCount)
	{
		constexpr float kHeatmapScale = 5.f;
		const float t = std::clamp(static_cast<float>(bounce) / kHeatmapScale, 0.f, 1.f);
		const float r = std::clamp(2.f * t - 1.f, 0.f, 1.f);
		const float g = t < 0.5f ? 2.f * t : 2.f * (1.f - t);
		const float b = std::clamp(1.f - 2.f * t, 0.f, 1.f);
		return ColorRgba(r, g, b, 1.f);
	}

	return accumulated_color;
}


ColorRgba SoftwarePathTracer::EnvironmentRadiance(const Scene* scene, const Vector3& direction)
{
	if (!scene->environment_map.has_value())
	{
		return scene->background_color;
	}

	const auto& env = scene->environment_map.value();
	Vector3 dir{ direction };

	if (env.flip_horizontal)
	{
		dir.x = -dir.x;
	}

	if (env.rotation_degrees != 0.f)
	{
		const float rot_rad{ env.rotation_degrees * (static_cast<float>(std::numbers::pi) / 180.f) };
		const float cos_r{ std::cos(rot_rad) };
		const float sin_r{ std::sin(rot_rad) };
		dir = Vector3(cos_r * dir.x + sin_r * dir.z, dir.y, -sin_r * dir.x + cos_r * dir.z);
	}

	return env.texture.SampleDirection(dir) * env.intensity;
}

bool SoftwarePathTracer::IsThinWalled(const SurfaceProperties& surface)
{
	return surface.thickness <= 0.f && surface.attenuation_distance >= std::numeric_limits<float>::max();
}

SoftwarePathTracer::BounceSample SoftwarePathTracer::SampleReflectOrRefract(const ShadingInput& input, const SurfaceProperties& surface, MediumState& medium, uint32_t& hash_state)
{
	const Vector3 incoming{ -input.view_direction };
	Vector3 normal{ input.world_normal };
	if (Vector3::Dot(incoming, normal) > 0.f)
	{
		normal = -normal;
	}

	if (IsThinWalled(surface))
	{
		const float fresnel_thin{ FresnelFunction_Schlick(normal, input.view_direction, surface.f0).r };
		if (NextFloat(hash_state) < fresnel_thin)
		{
			return { Vector3::Reflect(incoming, normal), ColorRgba::White() };
		}
		return { incoming, surface.albedo };
	}

	const float eta{ medium.inside ? surface.ior : 1.f / surface.ior };
	const Vector3 refracted{ Vector3::Refract(incoming, normal, eta) };
	const bool total_internal_reflection{ refracted.SqrMagnitude() < 1e-8f };

	const float fresnel{ total_internal_reflection ? 1.f : FresnelFunction_Schlick(normal, medium.inside ? -refracted : input.view_direction, surface.f0).r };

	if (total_internal_reflection || NextFloat(hash_state) < fresnel)
	{
		return { Vector3::Reflect(incoming, normal), ColorRgba::White() };
	}

	medium.inside = !medium.inside;
	medium.attenuation_color = medium.inside ? surface.attenuation_color : ColorRgba::White();
	medium.attenuation_distance = medium.inside ? surface.attenuation_distance : std::numeric_limits<float>::max();

	return { refracted, surface.albedo };
}

bool SoftwarePathTracer::IsAlphaSkipped(const SurfaceProperties& surface, uint32_t& hash_state)
{
	if (surface.transmission > 0.f)
	{
		return false;
	}

	switch (surface.alpha_params.mode)
	{
	case AlphaMode::kMask:
		return surface.albedo.a < surface.alpha_params.cutoff;
	case AlphaMode::kBlend:
		return NextFloat(hash_state) >= surface.albedo.a;
	case AlphaMode::kOpaque:
	default:
		return false;
	}
}

ColorRgba SoftwarePathTracer::ShadowTransmittance(const Ray& shadow_ray, const Scene* scene, uint32_t& hash_state) const
{
	ColorRgba transmittance{ ColorRgba::White() };
	Ray ray{ shadow_ray };
	RayHitRecord hit{};

	while (SceneClosestHitTest(ray, hit, false))
	{
		ShadingInput shading_input{ ConstructShadingInput(hit) };
		shading_input.view_direction = -ray.direction;

		const SceneObject& obj{ scene->objects.at(hit.object_index) };
		const Material* material{ scene->materials_factory.Get(obj.material_index) };
		const SurfaceProperties surface{ material->GetSurfaceProperties(shading_input, scene->textures_factory) };

		if (!IsAlphaSkipped(surface, hash_state))
		{
			if (surface.transmission > 0.f && IsThinWalled(surface))
			{
				Vector3 normal{ surface.shading_normal };
				if (Vector3::Dot(ray.direction, normal) > 0.f)
				{
					normal = -normal;
				}
				const float fresnel{ FresnelFunction_Schlick(normal, -ray.direction, surface.f0).r };
				transmittance *= surface.albedo * (1.f - fresnel);
			}
			else
			{
				return ColorRgba::Black();
			}

			if (transmittance.Luminance() < 0.001f)
			{
				return ColorRgba::Black();
			}
		}

		const float travelled{ hit.t + 0.001f };
		ray.origin += ray.direction * travelled;
		ray.max -= travelled;
	}

	return transmittance;
}