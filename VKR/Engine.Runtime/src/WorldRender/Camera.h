#pragma once

#include <Engine.Math/Scalar.h>
#include <Engine.Math/Vectors.h>
#include <Engine.Math/Matrix4.h>

struct GLFWwindow;
namespace Eng
{
	using namespace Math;

	class Input;
	class DebugLinePass;

	class Camera
	{
	public:

		enum class EProjectionType
		{
			PERSPECTIVE,
			ORTHOGRAPHIC
		};

		struct CreateDesc
		{
			Vector3f m_position;
			Vector3f m_eulerAngles;
			EProjectionType m_projectionType = EProjectionType::PERSPECTIVE;
			float m_sensitivity = 0.1f;
			float m_moveSpeed = 5.0f;
			float m_width = 1920.0f;
			float m_height = 1080.0f;
			float m_zNear = 0.1f;
			float m_zFar = 1000.0f;
			float m_fovY = 45.0f;
		};

		struct CameraFrustrum
		{
			CameraFrustrum() = default;

			inline CameraFrustrum& operator = (const CameraFrustrum& other)
			{
				std::memcpy(m_planes, other.m_planes, sizeof(CameraFrustrum::m_planes));
				std::memcpy(m_frustrumCorners, other.m_frustrumCorners, sizeof(CameraFrustrum::m_frustrumCorners));
				return *this;
			}

			using Plane = Vector4f;
			union
			{
				struct
				{
					Plane m_left;
					Plane m_right;
					Plane m_top;
					Plane m_bottom;
					Plane m_near;
					Plane m_far;
				};
				Plane m_planes[6]{};
			};

			union
			{
				struct
				{
					Vector3f m_nearTopLeft;
					Vector3f m_nearTopRight;
					Vector3f m_nearBottomLeft;
					Vector3f m_nearBottomRight;

					Vector3f m_farTopLeft;
					Vector3f m_farTopRight;
					Vector3f m_farBottomLeft;
					Vector3f m_farBottomRight;
				};
				Vector3f m_frustrumCorners[8]{};
			};
		};

		Camera();

		Camera(const CreateDesc& desc);

		~Camera() = default;

		/*
		Description: Update the camera transform & matrices to reflect any changes made since the last update.
		*/
		void Update();

		/*
		Description: Update the camera input and movement this frame.
		Param:
			float fDeltaTime: Time between frames.
			Input* input: Pointer to the input class of the application.
			GLFWwindow* window: Pointer to the window instance.
		*/
		void UpdateFreeCam(float fDeltaTime, Input* input, GLFWwindow* window);


		/*
		Description: Get the worldspace model matrix of the camera.
		Return Type: mat4
		*/
		Matrix4 GetWorldMatrix() const { return m_matrix; }

		/*
		Description: Get the view matrix of this camera.
		Return Type: mat4
		*/
		Matrix4 GetViewMatrix() const { return m_matrix.Inverse(); };

		Matrix4 GetProjectionMatrix() const;

		Matrix4 GetTransformMatrix() const { return m_matrix; }

		const CameraFrustrum& GetFrustrum() const { return m_frustrum; }

		Vector3f GetCursorNearPlaneWorldPosition(Vector2f cursorCoords);
		Vector3f GetCursorFarPlaneWorldPosition(Vector2f cursorCoords);

		void GetFrustrumSection(CameraFrustrum& outFrustrum, float nearDistance, float farDistance) const;

		static void GetShadowCascadeFrustrum(CameraFrustrum& outFrustrum, Vector3f position, Vector3f forward, float leftBound, float rightBound, float bottomBound, float topBound, float nearDistance, float farDistance);

		static void DrawFrustrum(DebugLinePass* linePass, const CameraFrustrum& frustrum, Vector3f color);

		Vector3f Position() const { return m_matrix[3]; }
		Vector3f EulerAngles() const { return m_eulerAngles; }
		Vector3f Right() const { return m_matrix[0]; }
		Vector3f Up() const { return m_matrix[1]; }
		Vector3f Forward() const { return m_matrix[2]; }
		float Aspect() const { return m_aspect; }
		float ZNear() const { return m_zNear; }
		float ZFar() const { return m_zFar; }
		float FovY() const { return m_fovY; }

		void SetPosition(Vector3f pos) { m_position = pos; }
		void Translate(Vector3f translation) { m_position += translation; }
		void SetRotation(Vector3f eulerAngles) { m_eulerAngles = eulerAngles; }
		void Rotate(Vector3f deltaEulerAngles) { m_eulerAngles += deltaEulerAngles; }
		void SetZNearDistance(float distance) { m_zNear = distance; }
		void SetZFarDistance(float distance) { m_zFar = distance; }
		void SetWidth(float width);
		void SetHeight(float height);

	private:

		CameraFrustrum m_frustrum;
		Matrix4 m_matrix;
		Vector3f m_position;
		Vector3f m_eulerAngles;
		EProjectionType m_projectionType = EProjectionType::PERSPECTIVE;
		float m_sensitivity = 0.1f;
		float m_moveSpeed = 5.0f;
		float m_lastMouseX = 0.0f;
		float m_lastMouseY = 0.0f;
		float m_width = 1920.0f;
		float m_height = 1080.0f;
		float m_aspect = 1920.0f / 1080.0f;
		float m_zNear = 0.1f;
		float m_zFar = 1000.0f;
		float m_fovY = 45.0f;
		bool m_bLooking = false;
	};

};