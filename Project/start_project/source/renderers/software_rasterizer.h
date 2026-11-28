//==============================================================
//  Graphics Programming 1 (2026-2027)
//  Authors : Matthieu Delaere
//  Copyright (c) 2026 Matthieu Delaere. All rights reserved.
//==============================================================
#ifndef SOFTWARE_RASTERIZER_HEADER
#define SOFTWARE_RASTERIZER_HEADER

//--- Standard Includes ---

//--- Framework Includes ---
#include <renderer.h>

namespace gfx
{
	//--- Software Rasterizer ---
	class SoftwareRasterizer final : public Renderer
	{
	public:
		//--- Construction / Destruction ---
		SoftwareRasterizer(Context* const context);
		~SoftwareRasterizer() override;

		SoftwareRasterizer(const SoftwareRasterizer&) = delete;
		SoftwareRasterizer& operator=(const SoftwareRasterizer&) = delete;
		SoftwareRasterizer(SoftwareRasterizer&&) = delete;
		SoftwareRasterizer& operator=(SoftwareRasterizer&&) = delete;

		//--- Public Functions ---
		void Render() override;
		
		static void NdcToScreen(Vector4& position, uint32_t width, uint32_t height);
		void RasterizeTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, uint32_t object_id, const Scene* scene, CullMode cull_mode);
		static bool PointInTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vector2& pixel, CullMode cull_mode, RasterHitRecord& hit_record);
	
		static void TransformMesh(TriangleMesh* mesh, const Matrix& world_matrix,const Matrix& view_matrix, const Matrix& projection_matrix);

		static bool NotInFrustum(const Vector4& ndc_position);

		template<typename T>
		static T InterpolateAttribute(const std::array<T, 3>& attributes, const Vector2& barycentric_coordinates)
		{
			const float w1{ barycentric_coordinates.x };
			const float w2{ barycentric_coordinates.y };
			const float w0{ 1.f - w1 - w2 };
			return attributes[0] * w0 + attributes[1] * w1 + attributes[2] * w2;
		}

		// --- Data Members ---

		std::vector<float> depth_buffer_{};
		static constexpr float kClearDepth{ 0.f };

	};
}
#endif //SOFTWARE_RASTERIZER_HEADER
