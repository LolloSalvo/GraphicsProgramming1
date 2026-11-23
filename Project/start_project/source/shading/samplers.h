#ifndef SAMPLERS_HEADER
#define SAMPLERS_HEADER

//--- Standard Includes ---
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

//--- Framework Includes ---
#include <vector3.h>
#include <brdfs.h>
#include <materials.h>

namespace gfx
{
	struct HemisphereSample
	{
		Vector3 direction{};
		float pdf{};
	};

	[[maybe_unused]]
	static uint32_t LowBias32(uint32_t h)
	{
		h ^= h >> 16; h *= 0x7feb352du;
		h ^= h >> 15; h *= 0x846ca68bu;
		h ^= h >> 16;
		return h;
	}

	[[maybe_unused]]
	static float HashToFloat(const uint32_t h)
	{
		return static_cast<float>(h >> 8) * (1.f / 16777216.f);
	}

	[[maybe_unused]]
	static Vector3 LocalToWorld(const Vector3& local_dir, const Vector3& normal)
	{
		const Vector3 helper{ std::abs(normal.x) > 0.9f ? Vector3{ 0.f, 1.f, 0.f } : Vector3{ 1.f, 0.f, 0.f } };
		const Vector3 tangent{ Vector3::Cross(normal, helper).Normalized() };
		const Vector3 bitangent{ Vector3::Cross(normal, tangent) };

		return tangent * local_dir.x + bitangent * local_dir.y + normal * local_dir.z;
	}

	[[maybe_unused]]
	static float NextFloat(uint32_t& h)
	{
		h = LowBias32(h);
		return HashToFloat(h);
	}

	
	[[maybe_unused]]
	static float PdfUniformHemisphere(const Vector3& normal, const Vector3& direction)
	{
		if (Vector3::Dot(normal, direction) <= 0.f)
		{
			return 0.f;
		}
		return 1.f / (2.f * static_cast<float>(std::numbers::pi));
	}
	[[maybe_unused]]

	static HemisphereSample SampleUniformHemisphere(const Vector3& normal, uint32_t& hash_state)
	{
		const float u1{ NextFloat(hash_state) };
		const float u2{ NextFloat(hash_state) };

		const float cos_theta{ u2 };
		const float sin_theta{ std::sqrt(std::max(0.f, 1.f - cos_theta * cos_theta)) };
		const float phi{ 2.f * static_cast<float>(std::numbers::pi) * u1 };

		const Vector3 local_dir{ std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, cos_theta };
		const Vector3 direction{ LocalToWorld(local_dir, normal) };

		return { direction, PdfUniformHemisphere(normal, direction) };
	}


	[[maybe_unused]]
	static float PdfCosineHemisphere(const Vector3& normal, const Vector3& direction)
	{
		const float cos_theta{ Vector3::Dot(normal, direction) };
		if (cos_theta <= 0.f)
		{
			return 0.f;
		}
		return cos_theta / static_cast<float>(std::numbers::pi);
	}
	[[maybe_unused]]

	static HemisphereSample SampleCosineHemisphere(const Vector3& normal, uint32_t& hash_state)
	{
		const float u1{ NextFloat(hash_state) };
		const float u2{ NextFloat(hash_state) };

		const float cos_theta{ std::sqrt(1.f - u2) };
		const float sin_theta{ std::sqrt(u2) };
		const float phi{ 2.f * static_cast<float>(std::numbers::pi) * u1 };

		const Vector3 local_dir{ sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta };
		const Vector3 direction{ LocalToWorld(local_dir, normal) };

		return { direction, PdfCosineHemisphere(normal, direction) };
	}

	[[maybe_unused]] static float PdfGGX(const Vector3& normal, const Vector3& view_dir,
		const Vector3& light_dir, const float roughness)
	{
		const Vector3 h{ (view_dir + light_dir).Normalized() };
		const float n_dot_h{ std::max(Vector3::Dot(normal, h), 0.0001f) };
		const float v_dot_h{ std::max(Vector3::Dot(view_dir, h), 0.0001f) };
		return NormalDistribution_GGX(normal, h, roughness) * n_dot_h / (4.f * v_dot_h);
	}

	[[maybe_unused]] static HemisphereSample SampleGGX(const Vector3& normal, const Vector3& view_dir,
		const float roughness, uint32_t& hash_state)
	{
		const float u1{ NextFloat(hash_state) };
		const float u2{ NextFloat(hash_state) };

		const float a{ roughness * roughness };
		const float a2{ a * a };

		const float cos_theta_h{ std::sqrt((1.f - u1) / (1.f + (a2 - 1.f) * u1)) };
		const float sin_theta_h{ std::sqrt(std::max(0.f, 1.f - cos_theta_h * cos_theta_h)) };
		const float phi{ 2.f * static_cast<float>(std::numbers::pi) * u2 };

		const Vector3 local_h{ sin_theta_h * std::cos(phi), sin_theta_h * std::sin(phi), cos_theta_h };
		const Vector3 h{ LocalToWorld(local_h, normal) };
		const Vector3 direction{ Vector3::Reflect(-view_dir, h) };

		return { direction, PdfGGX(normal, view_dir, direction, roughness) };
	}

	[[maybe_unused]] static float SpecularProbability(const ShadingInput& input,
		const SurfaceProperties& surface)
	{
		const float fresnel{ FresnelFunction_Schlick(input.world_normal, input.view_direction, surface.f0).r };
		const float probability{ std::clamp(fresnel, 0.05f, 0.95f) };
		return std::lerp(probability, 1.f, surface.metallic);
	}

	[[maybe_unused]] static float PdfSpecularOrDiffuse(const ShadingInput& input,
		const SurfaceProperties& surface, const Vector3& direction)
	{
		if (Vector3::Dot(input.world_normal, direction) <= 0.f)
		{
			return 0.f;
		}

		const float p{ SpecularProbability(input, surface) };
		return p * PdfGGX(input.world_normal, input.view_direction, direction, surface.roughness)
			+ (1.f - p) * PdfCosineHemisphere(input.world_normal, direction);
	}

	[[maybe_unused]] static HemisphereSample SampleSpecularOrDiffuse(const ShadingInput& input,
		const SurfaceProperties& surface, uint32_t& hash_state)
	{
		const float p{ SpecularProbability(input, surface) };
		const HemisphereSample sample{ NextFloat(hash_state) < p
			? SampleGGX(input.world_normal, input.view_direction, surface.roughness, hash_state)
			: SampleCosineHemisphere(input.world_normal, hash_state) };

		return { sample.direction, PdfSpecularOrDiffuse(input, surface, sample.direction) };
	}
}

#endif
