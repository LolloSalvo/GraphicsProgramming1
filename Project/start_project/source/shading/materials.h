//==============================================================
//  Graphics Programming 1 (2026-2027)
//  Authors : Matthieu Delaere
//  Copyright (c) 2026 Matthieu Delaere. All rights reserved.
//==============================================================
#ifndef MATERIALS_HEADER
#define MATERIALS_HEADER

//--- Standard Includes ---
#include <cassert>
#include <optional>

//--- Framework Includes ---
#include <vector2.h>
#include <vector3.h>
#include <color.h>
#include <brdfs.h>
#include <texture.h>
#include <factory.h>
#include <debug_views.h>

namespace gfx
{
	//--- Shading Input ---
	struct ShadingInput final
	{
		Vector3 view_direction{};
		Vector3 light_direction{};
		Vector3 world_normal{};
		Vector3 world_position{};
		Vector3 shadow_origin{};
		std::optional<Vector3> color{};
		std::optional<Vector3> world_tangent{};
		std::optional<Vector3> world_bitangent{};
		std::optional<Vector2> texture_coordinates{};
	};

	//--- Alpha Coverage ---
	enum class AlphaMode : uint8_t
	{
		kOpaque,
		kMask,
		kBlend
	};

	struct AlphaCoverageParams final
	{
		float cutoff{ 0.5f };
		AlphaMode mode{ AlphaMode::kOpaque };
	};

	//--- Material Parameters ---
	struct UnlitMaterialParams final
	{
		ColorRgba color{ ColorRgba::White() };
		std::optional<uint32_t> color_texture{};
		AlphaCoverageParams alpha_params{};
	};

	struct LambertMaterialParams final
	{
		ColorRgba diffuse_color{ ColorRgba::White() };
		float diffuse_reflectance{ 1.f };
		std::optional<uint32_t> diffuse_texture{};
		std::optional<uint32_t> normal_texture{};
		AlphaCoverageParams alpha_params{};
	};

	struct PbrMaterialParams final
	{
		ColorRgba albedo_color_factor{ ColorRgba::White() };
		float metallic_factor{ 0.f };
		float roughness_factor{ 1.f };
		float reflectance_factor{ 0.04f };
		float transmission_factor{ 0.f };
		float ior{ 1.5f };
		ColorRgba attenuation_color{ ColorRgba::White() };
		float attenuation_distance{ std::numeric_limits<float>::max() };
		float thickness_factor{ 0.f };
		std::optional<uint32_t> albedo_color_texture{};
		std::optional<uint32_t> metallic_roughness_texture{};
		std::optional<uint32_t> normal_texture{};
		std::optional<uint32_t> occlusion_texture{};
		std::optional<uint32_t> thickness_texture{};
		AlphaCoverageParams alpha_params{};
	};

	//--- Template Helper Functions ---
	// Use SFINAE for detecting if we have a normal map texture or not.
	template<typename T, typename = void>
	struct has_normal_texture : std::false_type {};

	template<typename T>
	struct has_normal_texture<T, std::void_t<decltype(std::declval<T>().normal_texture)>> :
		std::true_type {
	};

	template<typename ParamType>
	Vector3 SampleNormal(const ParamType& params, const ShadingInput& input,
		const Factory<Texture>& textures)
	{
		//TODO
		if constexpr (has_normal_texture<ParamType>::value)
		{
			if (params.normal_texture.has_value() && input.texture_coordinates.has_value()
				&& input.world_tangent.has_value() && input.world_bitangent.has_value())
			{
				const Texture* tex = textures.Get(params.normal_texture.value());
				if (tex)
				{
					const Vector3 tangent_normal = tex->SampleNormal(input.texture_coordinates.value());

					const Vector3 T = input.world_tangent.value();
					const Vector3 B = input.world_bitangent.value();
					const Vector3 N = input.world_normal;

					const Matrix tbn{ T, B, N, Vector3{} };

					return tbn.TransformVector(tangent_normal).Normalized();
				}
			}
		}
		return input.world_normal;
	}

	//--- Surface Properties Data ---
	struct SurfaceProperties final
	{
		ColorRgba albedo{ ColorRgba::White() };
		ColorRgba f0{ ColorRgba(.04f, .04f, .04f) };
		ColorRgba emissive{ ColorRgba::Black() };
		ColorRgba attenuation_color{ ColorRgba::White() };
		Vector3 shading_normal{};
		float roughness{ 1.f };
		float metallic{ 0.f };
		float reflectance{ 0.f };
		float transmission{ 0.f };
		float ior{ 1.5f };
		float occlusion{ 1.f };
		float attenuation_distance{ std::numeric_limits<float>::max() };
		float thickness{ 0.f };
		AlphaCoverageParams alpha_params{};
	};


	//--- Material Templating ---
	class Material
	{
	public:
		//--- Constructors & Destructor ---
		Material() = default;
		virtual ~Material() = default;
		Material(const Material&) = default;
		Material& operator=(const Material&) = default;
		Material(Material&&) = default;
		Material& operator=(Material&&) = default;

		//--- Pure Virtual Functions ---
		[[nodiscard]] virtual SurfaceProperties GetSurfaceProperties(const ShadingInput& input,
			const Factory<Texture>& textures) const = 0;
		[[nodiscard]] virtual ColorRgba Shade(const ShadingInput& input,
			const SurfaceProperties& surface) const = 0;
		[[nodiscard]] virtual AlphaMode GetAlphaMode() const = 0;
	};

	template<typename ParamType>
	class MaterialTemplate : public Material
	{
	protected:
		ParamType params_;

	public:
		//--- Constructors & Destructor ---
		MaterialTemplate() = default;
		explicit MaterialTemplate(const ParamType& params) : params_(params) {}
		explicit MaterialTemplate(ParamType&& params) : params_(std::move(params)) {}

		//--- Public Functions ---
		[[nodiscard]] const ParamType& GetParams() const { return params_; }
		[[nodiscard]] AlphaMode GetAlphaMode() const override 
		{ return params_.alpha_params.mode; }

		//--- Pure Virtual Functions ---
		[[nodiscard]] SurfaceProperties GetSurfaceProperties(const ShadingInput& input,
			const Factory<Texture>& textures) const override = 0;
		[[nodiscard]] ColorRgba Shade(const ShadingInput& input,
			const SurfaceProperties& surface) const override = 0;
	};

	//--- Materials ---
	class UnlitMaterial final : public MaterialTemplate<UnlitMaterialParams>
	{
	public:
		//--- Constructors ---
		UnlitMaterial() = default;
		explicit UnlitMaterial(const UnlitMaterialParams& params) : MaterialTemplate(params) {}
		explicit UnlitMaterial(UnlitMaterialParams&& params) : MaterialTemplate(std::move(params)) {}

		//--- Overrides ---
		[[nodiscard]] SurfaceProperties GetSurfaceProperties(const ShadingInput& input, 
			const Factory<Texture>& textures) const override
		{
			//TODO
			assert(false && "Not Implemented");
			(void)input; (void)textures;
			return {};
		}

		[[nodiscard]] ColorRgba Shade(const ShadingInput& input,
			const SurfaceProperties& surface) const override
		{
			//TODO
			assert(false && "Not Implemented");
			(void)input; (void)surface;
			return {};
		}
	};

	class LambertMaterial final : public MaterialTemplate<LambertMaterialParams>
	{
	public:
		//--- Constructors ---
		LambertMaterial() = default;
		explicit LambertMaterial(const LambertMaterialParams& params) : MaterialTemplate(params) {}
		explicit LambertMaterial(LambertMaterialParams&& params) : MaterialTemplate(std::move(params)) {}

		//--- Overrides ---
		[[nodiscard]] SurfaceProperties GetSurfaceProperties(const ShadingInput& input,
			const Factory<Texture>& textures) const override
		{
			(void)textures;

			SurfaceProperties surface{};

			surface.albedo = params_.diffuse_color * params_.diffuse_reflectance;
			surface.shading_normal = input.world_normal;
			
			return surface;
		}

		[[nodiscard]] ColorRgba Shade(const ShadingInput& input,
			const SurfaceProperties& surface) const override
		{
			(void)input;

			return Lambert(params_.diffuse_reflectance, surface.albedo);
		}
	};

	class PbrMaterial final : public MaterialTemplate<PbrMaterialParams>
	{
	public:
		//--- Constructors ---
		PbrMaterial() = default;
		explicit PbrMaterial(const PbrMaterialParams& params) : MaterialTemplate(params) {}
		explicit PbrMaterial(PbrMaterialParams&& params) : MaterialTemplate(std::move(params)) {}

		//--- Overrides ---
		[[nodiscard]] SurfaceProperties GetSurfaceProperties(const ShadingInput& input,
			const Factory<Texture>& textures) const override
		{
			SurfaceProperties surface{};

			surface.albedo = params_.albedo_color_factor;

			surface.roughness = params_.roughness_factor;
			surface.metallic = params_.metallic_factor;

			if (input.texture_coordinates.has_value())
			{
				Vector2 uv{ input.texture_coordinates.value() };

				if (params_.albedo_color_texture.has_value())
				{
					const Texture* tex = textures.Get(params_.albedo_color_texture.value());

					if (tex)
					{
						const ColorRgba sample = tex->Sample(uv);
						surface.albedo *= (tex->IsSrgb() ? sample.ToLinear() : sample);
					}
				}

				if (params_.metallic_roughness_texture.has_value())
				{
					const Texture* mr_tex = textures.Get(params_.metallic_roughness_texture.value());

					if (mr_tex)
					{
						const ColorRgba sample = mr_tex->Sample(uv);
					
						surface.roughness *= sample.g;
						surface.metallic *= sample.b;
					}
				}
			}

			surface.shading_normal = SampleNormal(params_, input, textures);

			const float ior{ params_.ior };
			const float f0_dielectric{ ((ior - 1.f) / (ior + 1.f)) * ((ior - 1.f) / (ior + 1.f)) };
			const ColorRgba f0_color{ f0_dielectric, f0_dielectric, f0_dielectric };

			surface.f0 = ColorRgba::Lerp(f0_color, surface.albedo, surface.metallic);

			return surface;
		}

		[[nodiscard]] ColorRgba Shade(const ShadingInput& input,
			const SurfaceProperties& surface) const override
		{
			const Vector3 v{ input.view_direction };
			const Vector3 l{ input.light_direction };
			const Vector3 n{ surface.shading_normal };
			const Vector3 h{ (v + l).Normalized() };

			const ColorRgba F{ FresnelFunction_Schlick(h, v, surface.f0) };
			const float D{ NormalDistribution_GGX(n, h, surface.roughness) };
			const float G{ GeometryFunction_Smith(n, v, l, surface.roughness) };

			float denominator{ 4.f * std::max(0.f, Vector3::Dot(n, v)) * std::max(0.f, Vector3::Dot(n, l)) };
			denominator = std::max(denominator, 0.001f);

			const ColorRgba specular{ (F * D * G) / denominator };

			ColorRgba kd{ ColorRgba{1.f, 1.f, 1.f} - F };
			kd *= 1.f - surface.metallic;

			return Lambert(kd, surface.albedo) + specular;
		}
	};
}
#endif //MATERIALS_HEADER