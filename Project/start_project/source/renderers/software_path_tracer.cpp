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

	int surface_height{ static_cast<int>(surface_info.height) };
	int surface_width{ static_cast<int>(surface_info.width) };

	Scene* active_scene{ context_->scene_manager->GetActiveScene() };
	DebugParams debug_params{ context_->debug_params };
	VisualizationMode vis_mode{ debug_params.visualization_mode };

#pragma omp parallel for schedule(dynamic, 16)
	for (int py{}; py < surface_height; ++py)
	{
		for (int px{}; px < surface_width; ++px)
		{

			float x{ (2 * ((px + 0.5f) / surface_info.width) - 1) * aspectRatio * fov } ;
			float y{ (1 - 2 * ((py + 0.5f) / surface_info.height)) * fov };

			Vector3 ray_direction_camera_space{ x,y,1.f };

			Vector3 world_ray_direction{ world_matrix.TransformVector(ray_direction_camera_space) };
			world_ray_direction.Normalize();
			 
			Ray world_ray{ active_scene->camera.GetPosition(), world_ray_direction };

			ColorRgba final_color{ 0.f, 0.f, 0.f};

			RayHitRecord closest_hit_record{};



			if (SceneClosestHitTest(world_ray, closest_hit_record, false))
			{
				ShadingInput shading_input = ConstructShadingInput(closest_hit_record);
				shading_input.view_direction = -world_ray_direction;

				const SceneObject& obj{ active_scene->objects.at(closest_hit_record.object_index) };
				const Material* material{ active_scene->materials_factory.Get(obj.material_index) };
				const SurfaceProperties surface{ material->GetSurfaceProperties(shading_input, active_scene->textures_factory) };

				shading_input.world_normal = surface.shading_normal;
				
				if (debug_params.visualization_mode == VisualizationMode::kDepth)
				{
					const float max_depth{ 100.f };
					const float scaled_t{ 1.f -
						std::clamp(closest_hit_record.t / max_depth, 0.f, 1.f) };
					final_color = { scaled_t, scaled_t, scaled_t };
				}
				else if (vis_mode == VisualizationMode::kNormals)
				{
					final_color = {
						(shading_input.world_normal.x + 1.f) * 0.5f,
						(shading_input.world_normal.y + 1.f) * 0.5f,
						(shading_input.world_normal.z + 1.f) * 0.5f };
				}
				else if (vis_mode == VisualizationMode::kAlbedo)
				{
					final_color = surface.albedo;
				}
				else if (vis_mode == VisualizationMode::kMetallicRoughness)
				{
					final_color = { 0.f, surface.roughness, surface.metallic };
				}
				else if (vis_mode == VisualizationMode::kUV)
				{
					if (shading_input.texture_coordinates.has_value())
					{
						final_color = {
							shading_input.texture_coordinates.value().x,
							shading_input.texture_coordinates.value().y,
							0.f                                          
						};
					}
					else
					{
						final_color = { 0.f, 0.f, 0.f }; // Nero se la mesh colpita non ha UV map
					}
				}
				else if (vis_mode == VisualizationMode::kDirectOnly)
				{
					Vector3 light_dir{};
					for (Light* light : active_scene->lights_factory.GetAll())
					{
						if (light->type == LightType::kDirectional)
						{
							light_dir = -light->direction;
						}
						else
						{
							light_dir = light->origin - shading_input.world_position;
						}

						const float light_distance{ light_dir.Normalize() };
						//light_dir.Normalize();

						const float cos_theta{ Vector3::Dot(shading_input.world_normal, light_dir) };
						if (cos_theta <= 0.f)
						{
							continue;
						}

						const Ray shadow_ray{
							.origin = shading_input.shadow_origin,
							.direction = light_dir,
							.min = 0.001f,
							.max = light->type == LightType::kDirectional
								? std::numeric_limits<float>::max() : light_distance
						};
						RayHitRecord shadow_hit{};

						if (SceneClosestHitTest(shadow_ray, shadow_hit, true))
						{
							continue;
						}

						shading_input.light_direction = light_dir;
						
						ColorRgba radiance = CalculateRadiance(light, shading_input.world_position);

						ColorRgba brdf{ material->Shade(shading_input, surface) };

						final_color += radiance * brdf * cos_theta;

					}

					final_color = final_color.Tonemap(debug_params.tone_map_operator);
					final_color = final_color.ToSrgb();

					final_color.Clamp();

				}
				else if (vis_mode == VisualizationMode::kNone)
				{
					Vector3 light_dir{};
					for (Light* light : active_scene->lights_factory.GetAll())
					{
						if (light->type == LightType::kDirectional)
						{
							light_dir = -light->direction;
						}
						else
						{
							light_dir = light->origin - shading_input.world_position;
						}

						const float light_distance{ light_dir.Normalize() };
						//light_dir.Normalize();

						const float cos_theta{ Vector3::Dot(shading_input.world_normal, light_dir) };
						if (cos_theta <= 0.f)
						{
							continue;
						}

						const Ray shadow_ray{
							.origin = shading_input.shadow_origin,
							.direction = light_dir,
							.min = 0.001f,
							.max = light->type == LightType::kDirectional
								? std::numeric_limits<float>::max() : light_distance
						};
						RayHitRecord shadow_hit{};

						if (SceneClosestHitTest(shadow_ray, shadow_hit, true))
						{
							continue;
						}

						shading_input.light_direction = light_dir;

						ColorRgba radiance = CalculateRadiance(light, shading_input.world_position);

						ColorRgba brdf{ material->Shade(shading_input, surface) };

						final_color += radiance * brdf * cos_theta;

					}

					final_color = final_color.Tonemap(debug_params.tone_map_operator);
					final_color = final_color.ToSrgb();

					final_color.Clamp();

				}
			}


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
			if (HitTestTriangle(static_cast<const Triangle&>(*primitive), ray_model_space, temp_hit_record, ignore_record))
			{
				hit_primitive = true;
			}
			break;
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

						const Vector3 edge1 = triangle.v1 - triangle.v0;
						const Vector3 edge2 = triangle.v2 - triangle.v0;
						triangle.normal = Vector3::Cross(edge1, edge2).Normalized();

						if (HitTestTriangle(
							triangle,
							ray_model_space,
							temp_hit_record,
							ignore_record))
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

ColorRgba SoftwarePathTracer::CalculateRadiance(Light* light, const Vector3& world_pos)
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