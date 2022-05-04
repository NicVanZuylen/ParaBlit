#pragma once

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT // Required to ensure glm constructors actually initialize vectors/matrices etc.
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#pragma warning(pop)

class Input;

struct GLFWwindow;

class Camera
{
public:

	struct CreateDesc
	{
		glm::vec3 m_position;
		glm::vec3 m_eulerAngles;
		float m_sensitivity = 0.1f;
		float m_moveSpeed = 5.0f;
		uint32_t m_width = 1920;
		uint32_t m_height = 1080;
		float m_zNear = 0.1f;
		float m_zFar = 1000.0f;
		float m_fovY = 45.0f;
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
	void SetWidth(uint32_t width);
	void SetHeight(uint32_t height);

private:

	glm::mat4 m_matrix;
	glm::vec3 m_position;
	glm::vec3 m_eulerAngles;
	float m_sensitivity = 0.1f;
	float m_moveSpeed = 5.0f;
	float m_lastMouseX = 0.0f;
	float m_lastMouseY = 0.0f;
	uint32_t m_width = 1920;
	uint32_t m_height = 1080;
	float m_aspect = 1920.0f / 1080.0f;
	float m_zNear = 0.1f;
	float m_zFar = 1000.0f;
	float m_fovY = 45.0f;
	bool m_bLooking = false;
};

