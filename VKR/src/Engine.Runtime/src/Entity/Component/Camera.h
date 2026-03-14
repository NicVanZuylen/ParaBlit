#pragma once

#include "Engine.Math/Vector3.h"
#include "Entity/Entity.h"
#include "Camera_generated.h"
#include "Entity/Component/Transform.h"
#include "Engine.Reflectron/ReflectronAPI.h"

#include <Engine.Math/Scalar.h>
#include <Engine.Math/Vectors.h>
#include <Engine.Math/Matrix4.h>

struct GLFWwindow;
namespace Eng
{
	using namespace Math;

	class Input;
	class DebugLinePass;

	class Camera : public EntityComponent
	{
		REFLECTRON_CLASS()

	public:

		REFLECTRON_GENERATED_Camera()

		enum class EProjectionType
		{
			PERSPECTIVE,
			ORTHOGRAPHIC
		};

		struct CreateDesc
		{
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

			Plane m_planes[6]{};

			Plane& m_left = m_planes[0];
			Plane& m_right = m_planes[1];
			Plane& m_top = m_planes[2];
			Plane& m_bottom = m_planes[3];
			Plane& m_near = m_planes[4];
			Plane& m_far = m_planes[5];
			

			Vector3f m_frustrumCorners[8]{};

			Vector3f& m_nearTopLeft = m_frustrumCorners[0];
			Vector3f& m_nearTopRight = m_frustrumCorners[1];
			Vector3f& m_nearBottomLeft = m_frustrumCorners[2];
			Vector3f& m_nearBottomRight = m_frustrumCorners[3];

			Vector3f& m_farTopLeft = m_frustrumCorners[4];
			Vector3f& m_farTopRight = m_frustrumCorners[5];
			Vector3f& m_farBottomLeft = m_frustrumCorners[6];
			Vector3f& m_farBottomRight = m_frustrumCorners[7];
		};

		Camera();

		Camera(const CreateDesc& desc);

		~Camera() = default;

		void OnInitialize() override;

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

		static void GetShadowCascadeFrustrum(CameraFrustrum& outFrustrum, Vector3f position, Vector3f forward, Vector3f trueUp, float leftBound, float rightBound, float bottomBound, float topBound, float nearDistance, float farDistance);

		static void DrawFrustrum(DebugLinePass* linePass, const CameraFrustrum& frustrum, Vector3f color);

		Vector3f Position() const { return m_matrix[3]; }
		Vector3f EulerAngles() const { return m_transform->GetEulerAngles(); }
		Vector3f Right() const { return m_matrix[0]; }
		Vector3f Up() const { return m_matrix[1]; }
		Vector3f Forward() const { return m_matrix[2]; }
		float Aspect() const { return m_aspect; }
		float ZNear() const { return m_zNear; }
		float ZFar() const { return m_zFar; }
		float FovY() const { return m_fovY; }

		void SetZNearDistance(float distance) { m_zNear = distance; }
		void SetZFarDistance(float distance) { m_zFar = distance; }
		void SetWidth(float width);
		void SetHeight(float height);

	private:

		REFLECTRON_FIELD(enum)
		EProjectionType m_projectionType = EProjectionType::PERSPECTIVE;

		REFLECTRON_FIELD(min=0.01, max=5.0)
		float m_sensitivity = 0.1f;

		REFLECTRON_FIELD()
		float m_moveSpeed = 5.0f;

		REFLECTRON_FIELD()
		float m_width = 1920.0f;

		REFLECTRON_FIELD()
		float m_height = 1080.0f;

		REFLECTRON_FIELD(min=0.01, max=0.5)
		float m_zNear = 0.1f;

		REFLECTRON_FIELD(min=500.0, max=10000.0)
		float m_zFar = 1000.0f;

		REFLECTRON_FIELD(min=1.0, max=110.0)
		float m_fovY = 45.0f;

		Transform* m_transform = nullptr;
		CameraFrustrum m_frustrum;
		Matrix4 m_matrix;
		float m_lastMouseX = 0.0f;
		float m_lastMouseY = 0.0f;
		float m_aspect = 1920.0f / 1080.0f;
		bool m_bLooking = false;
	};
	CLIB_REFLECTABLE_CLASS(Camera)

};