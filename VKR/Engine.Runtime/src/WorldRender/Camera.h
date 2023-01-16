#pragma once

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT // Required to ensure glm constructors actually initialize vectors/matrices etc.
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#pragma warning(pop)

struct GLFWwindow;
namespace Eng
{
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
			glm::vec3 m_position;
			glm::vec3 m_eulerAngles;
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
			CameraFrustrum() {};

			using Plane = glm::vec4;
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
					glm::vec3 m_nearTopLeft;
					glm::vec3 m_nearTopRight;
					glm::vec3 m_nearBottomLeft;
					glm::vec3 m_nearBottomRight;

					glm::vec3 m_farTopLeft;
					glm::vec3 m_farTopRight;
					glm::vec3 m_farBottomLeft;
					glm::vec3 m_farBottomRight;
				};
				glm::vec3 m_frustrumCorners[8]{};
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
		glm::mat4 GetWorldMatrix() const { return m_matrix; }

		/*
		Description: Get the view matrix of this camera.
		Return Type: mat4
		*/
		glm::mat4 GetViewMatrix() const { return glm::inverse(m_matrix); };

		glm::mat4 GetProjectionMatrix() const;

		glm::mat4 GetTransformMatrix() const { return m_matrix; }

		const CameraFrustrum& GetFrustrum() const { return m_frustrum; }

		void GetFrustrumSection(CameraFrustrum& outFrustrum, float nearDistance, float farDistance) const;

		static void GetShadowCascadeFrustrum(CameraFrustrum& outFrustrum, glm::vec3 position, glm::vec3 forward, float leftBound, float rightBound, float bottomBound, float topBound, float nearDistance, float farDistance);

		static void DrawFrustrum(DebugLinePass* linePass, const CameraFrustrum& frustrum, glm::vec3 color);

		glm::vec3 Position() const { return m_matrix[3]; }
		glm::vec3 EulerAngles() const { return m_eulerAngles; }
		glm::vec3 Right() const { return m_matrix[0]; }
		glm::vec3 Up() const { return m_matrix[1]; }
		glm::vec3 Forward() const { return m_matrix[2]; }
		float Aspect() const { return m_aspect; }
		float ZNear() const { return m_zNear; }
		float ZFar() const { return m_zFar; }
		float FovY() const { return m_fovY; }

		void SetPosition(glm::vec3 pos) { m_position = pos; }
		void Translate(glm::vec3 translation) { m_position += translation; }
		void SetRotation(glm::vec3 eulerAngles) { m_eulerAngles = eulerAngles; }
		void Rotate(glm::vec3 deltaEulerAngles) { m_eulerAngles += deltaEulerAngles; }
		void SetZNearDistance(float distance) { m_zNear = distance; }
		void SetZFarDistance(float distance) { m_zFar = distance; }
		void SetWidth(float width);
		void SetHeight(float height);

	private:

		CameraFrustrum m_frustrum;
		glm::mat4 m_matrix;
		glm::vec3 m_position;
		glm::vec3 m_eulerAngles;
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