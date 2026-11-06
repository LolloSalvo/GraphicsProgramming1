//==============================================================
//  Graphics Programming 1 (2026-2027)
//  Authors : Matthieu Delaere
//  Copyright (c) 2026 Matthieu Delaere. All rights reserved.
//==============================================================
#ifndef CAMERA_HEADER
#define CAMERA_HEADER

//--- Standard Includes ---
#include <cassert>
#include <numbers>
#include <iostream>

//--- Framework Includes ---
#include <matrix.h>
#include <vector3.h>

namespace gfx
{
	class Camera final
	{
		//--- Data members ---
		Matrix view_{ Matrix::CreateIdentity() };
		Matrix projection_{ Matrix::CreateIdentity() };

		Vector4 position_{ Vector4(Vector3::Zero(), 1.f) };
		Vector4 right_{ Vector4::UnitX() };
		Vector4 up_{ Vector4::UnitY() };
		Vector4 forward_{ Vector4::UnitZ() };

		float fov_angle_{ 60.f };
		float near_plane_{ 0.1f };
		float far_plane_{ 1000.f };
		bool dirty_{ true };

	public:
		//--- Constructors & Destructor ---
		Camera(const Vector3& position = Vector3::Zero(), const float fov_angle = 60.f) :
			position_(position, 1.f), fov_angle_(fov_angle) {
			UpdateView();
		}
		~Camera() = default;
		Camera(const Camera&) = default;
		Camera& operator=(const Camera&) = default;
		Camera(Camera&&) = default;
		Camera& operator=(Camera&&) = default;

		//--- Functions ---
		void MoveForward(const float delta)
		{
			position_ += forward_ * delta; UpdateView(); dirty_ = true;
		}
		void MoveRight(const float delta)
		{
			position_ += right_ * delta; UpdateView(); dirty_ = true;
		}
		void MoveUp(const float delta)
		{
			position_ += Vector4::UnitY() * delta; UpdateView(); dirty_ = true;
		}

		void Pitch(const float angle)
		{
			const Matrix rotation = Matrix::CreateRotationAxis(right_, angle);
			forward_ = rotation.TransformVector(forward_);
			up_ = rotation.TransformVector(up_);
			UpdateView();
			dirty_ = true;
		}
		void Yaw(const float angle)
		{
			const Matrix rotation = Matrix::CreateRotationY(angle);
			forward_ = rotation.TransformVector(forward_);
			right_ = rotation.TransformVector(right_);
			UpdateView();
			dirty_ = true;
		}

		void SetPosition(const Vector3& position) { position_ = Vector4(position, 1.f); UpdateView(); dirty_ = true; }
		void SetFovAngle(const float fov_angle) { fov_angle_ = fov_angle; dirty_ = true; }

		bool IsDirty() const { return dirty_; }
		void ClearDirty() { dirty_ = false; }

		const Vector4& GetPosition() const { return position_; }
		const Vector4& GetForward() const { return forward_; }
		float GetFovAngle() const { return fov_angle_; }

		// Pure read: safe to call from multiple threads (view_ is only rebuilt by the mutators).
		const Matrix& GetView() const { return view_; }
		const Matrix& GetProjection(const float fov_y, const float aspect_ratio)
		{
			//TODO: create projection matrix
			(void)fov_y; (void)aspect_ratio;
			(void)near_plane_; (void)far_plane_;
			return projection_;
		}

	private:
		void UpdateView()
		{
			// 1. Estrai i Vector3 dai Vector4 per eseguire la matematica
			Vector3 f3{ forward_.x, forward_.y, forward_.z };
			f3 = f3.Normalized();

			// 2. Ri-ortogonalizza la base (ONB)
			Vector3 world_up{ 0.f, 1.f, 0.f };
			Vector3 r3 = Vector3::Cross(world_up, f3).Normalized();
			Vector3 u3 = Vector3::Cross(f3, r3).Normalized();

			// 3. Aggiorna i Vector4 della classe (w = 0.f per i vettori di direzione)
			forward_ = Vector4{ f3.x, f3.y, f3.z, 0.f };
			right_ = Vector4{ r3.x, r3.y, r3.z, 0.f };
			up_ = Vector4{ u3.x, u3.y, u3.z, 0.f };

			// 4. Calcola la View Matrix con CreateLookAtLH
			Vector3 p3{ position_.x, position_.y, position_.z };
			view_ = Matrix::CreateLookAtLH(p3, f3, u3);
		}
	};
}
#endif //CAMERA_HEADER