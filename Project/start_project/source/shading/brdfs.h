//==============================================================
//  Graphics Programming 1 (2026-2027)
//  Authors : Matthieu Delaere
//  Copyright (c) 2026 Matthieu Delaere. All rights reserved.
//==============================================================
#ifndef BRDFS_HEADER
#define BRDFS_HEADER

//--- Standard Includes ---
#include <cassert>
#include <numbers>

//--- Framework Includes ---
#include <vector2.h>
#include <vector3.h>
#include <color.h>

namespace gfx
{
	//--- BRDFs ---
	/**
	* \param kd Diffuse Reflection Coefficient
	* \param cd Diffuse Color
	*/
	[[maybe_unused]]
	static ColorRgba Lambert(const float kd, const ColorRgba& cd)
	{
		return cd * kd / static_cast<float>(std::numbers::pi);
	}

	[[maybe_unused]]
	static ColorRgba Lambert(const ColorRgba& kd, const ColorRgba& cd)
	{
		return cd * kd / static_cast<float>(std::numbers::pi);
	}

	/**
	* \brief BRDF Fresnel Function >> Schlick
	* \param h Normalized Half vector between View and Light directions
	* \param v Normalized View direction
	* \param f0 Base reflectivity of a surface based on IOR (Indices Of Refraction),
				this is different for Dielectrics (Non-Metal) and Conductors (Metal)
	*/
	[[maybe_unused]]
	static ColorRgba FresnelFunction_Schlick(const Vector3& h, const Vector3& v, const ColorRgba& f0)
	{
		return f0 + (ColorRgba{1.f, 1.f, 1.f} - f0) * pow(1 - std::max(0.f, (Vector3::Dot(h, v))), 5.f);
	}

	/**
	* \brief BRDF Normal Distribution >> Trowbridge-Reitz GGX (UE4 implemetation - squared(roughness))
	* \param n Surface normal
	* \param h Normalized half vector
	* \param roughness Roughness of the material
	*/
	[[maybe_unused]]
	static float NormalDistribution_GGX(const Vector3& n, const Vector3& h, const float roughness)
	{
		float alpha_squared{ (roughness * roughness) * (roughness * roughness) };

		float dot_nh{ std::max(0.f, Vector3::Dot(n, h)) };
		float dot_nh_squared{ dot_nh * dot_nh };

		float denominator{ static_cast<float>(std::numbers::pi) * ((dot_nh_squared * (alpha_squared - 1) + 1) * (dot_nh_squared * (alpha_squared - 1) + 1)) };

		return alpha_squared / denominator;
	}

	/**
	* \brief BRDF Geometry Function >> Schlick GGX (Direct Lighting + UE4 implementation - squared(roughness))
	* \param n Normal of the surface
	* \param v Normalized direction (view or light)
	* \param roughness Roughness of the material
	* \param is_indirect If true, should use indirect k remapping
	*/
	[[maybe_unused]]
	static float GeometryFunction_SchlickGGX(const Vector3& n, const Vector3& v, const float roughness,
		[[maybe_unused]] const bool is_indirect = false)
	{
		float dot_nv{ std::max(0.f, Vector3::Dot(n,v)) };
		float k{};

		if (is_indirect)
		{
			k = (roughness * roughness) / 2.f;
		}
		else
		{
			k = ((roughness + 1.f) * (roughness + 1.f)) / 8.f;
		}

		return dot_nv / (dot_nv * (1 - k) + k);
	}

	/**
	* \brief BRDF Geometry Function >> Smith (separable form)
	* \param n Normal of the surface
	* \param v Normalized view direction
	* \param l Normalized light direction
	* \param roughness Roughness of the material
	* \param is_indirect (see GeometryFunction_SchlickGGX).
	*/
	[[maybe_unused]]
	static float GeometryFunction_Smith(const Vector3& n, const Vector3& v, const Vector3& l, const float roughness,
		[[maybe_unused]] const bool is_indirect = false)
	{
		float masking{ GeometryFunction_SchlickGGX(n, v, roughness, is_indirect) };
		float shadowing{ GeometryFunction_SchlickGGX(n, l, roughness, is_indirect) };

		return masking * shadowing;
	}
}
#endif