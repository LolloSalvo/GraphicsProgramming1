//==============================================================
//  Graphics Programming 1 (2026-2027)
//  Authors : Matthieu Delaere
//  Copyright (c) 2026 Matthieu Delaere. All rights reserved.
//==============================================================
#ifndef SOFTWARE_PATH_TRACER_HEADER
#define SOFTWARE_PATH_TRACER_HEADER

//--- Standard Includes ---

//--- Framework Includes ---
#include <renderer.h>

namespace gfx
{
	//--- Software Path Tracer ---
	class SoftwarePathTracer final : public Renderer
	{
	public:
		//--- Construction / Destruction ---
		SoftwarePathTracer(Context* const context);
		~SoftwarePathTracer() override;

		SoftwarePathTracer(const SoftwarePathTracer&) = delete;
		SoftwarePathTracer& operator=(const SoftwarePathTracer&) = delete;
		SoftwarePathTracer(SoftwarePathTracer&&) = delete;
		SoftwarePathTracer& operator=(SoftwarePathTracer&&) = delete;

		//--- Public Functions ---
		void Render() override;

		bool SceneClosestHitTest(const Ray& ray, RayHitRecord& closest_hit, bool ignore_record = false, bool reverse_culling = false) const;
		ShadingInput ConstructShadingInput(const RayHitRecord& hit) const;

		//--- Private Function
	private:

		struct BounceSample
		{
			Vector3 direction{};
			ColorRgba weight{ ColorRgba::Black() };
		};

		struct MediumState
		{
			bool inside{ false };
			ColorRgba attenuation_color{ ColorRgba::White() };
			float attenuation_distance{ std::numeric_limits<float>::max() };
		};

		ColorRgba DirectIllumination(const ShadingInput& shading_input, const SurfaceProperties& surface, const Material* material, const Scene* scene, uint32_t& hash_state) const;
		ColorRgba CalculateRadiance(Light* light, const Vector3& world_pos) const;

		Vector2 GenerateSubpixelJitter(uint32_t pixel_index, uint32_t& hash_state) const;
		
		ColorRgba TracePath(const Ray& ray, const Scene* scene, uint32_t& hash_state, int max_bounces = 8) const;

		static ColorRgba EnvironmentRadiance(const Scene* scene, const Vector3& direction);

		static bool IsThinWalled(const SurfaceProperties& surface);
		static BounceSample SampleReflectOrRefract(const ShadingInput& input, const SurfaceProperties& surface, MediumState& medium, uint32_t& hash_state);

		static bool IsAlphaSkipped(const SurfaceProperties& surface, uint32_t& hash_state);
		ColorRgba ShadowTransmittance(const Ray& shadow_ray, const Scene* scene, uint32_t& hash_state) const;

		VisualizationMode last_vis_mode_{ VisualizationMode::kNone };
		SamplingStrategy last_sampling_strategy_{ SamplingStrategy::kCosine };
		std::vector<ColorRgba> accumulation_buffer_{};

		ToneMapOperator last_tone_map_operator_{ ToneMapOperator::kACESHill };

		

	};
}
#endif //SOFTWARE_PATH_TRACER_HEADER
