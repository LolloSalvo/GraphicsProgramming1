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
	struct HemisphereSample
	{
		Vector3 direction{};
		float pdf{};
	};

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

		bool SceneClosestHitTest(const Ray& ray, RayHitRecord& closest_hit, bool ignore_record) const;
		ShadingInput ConstructShadingInput(const RayHitRecord& hit) const;

		float TraceAO(const ShadingInput& shading_input, uint32_t pixel_index) const;
		static HemisphereSample UniformSampleHemisphere(const Vector3& normal, uint32_t seed);
		static HemisphereSample CosineSampleHemisphere(const Vector3& normal, uint32_t seed);

		//--- Private Function
	private:

		ColorRgba DirectIllumination(const ShadingInput& shading_input, const SurfaceProperties& surface, const Material* material, const Scene* scene) const;
		ColorRgba CalculateRadiance(Light* light, const Vector3& world_pos) const;

		Vector2 GenerateSubpixelJitter(uint32_t pixel_index) const;

		uint32_t ao_sample_count_{ 1 };
		float ao_radius_{ 10.f };

		VisualizationMode last_vis_mode_{ VisualizationMode::kNone };
		SamplingStrategy last_sampling_strategy_{ SamplingStrategy::kCosine };
		std::vector<ColorRgba> accumulation_buffer_{};

	};
}
#endif //SOFTWARE_PATH_TRACER_HEADER
