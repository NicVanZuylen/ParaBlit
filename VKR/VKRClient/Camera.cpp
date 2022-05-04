#include "Camera.h"
#include "Input.h"
#include "GLFW/glfw3.h"

#include <iostream>

Camera::Camera()
{
}

Camera::Camera(const CreateDesc& desc) 
{
	m_sensitivity = desc.m_sensitivity;
	m_moveSpeed = desc.m_moveSpeed;
	m_bLooking = false;
	m_position = desc.m_position;
	m_eulerAngles = desc.m_eulerAngles;
	m_aspect = float(desc.m_width) / float(desc.m_height);
	m_zFar = desc.m_zNear;
	m_zFar = desc.m_zFar;
	m_fovY = desc.m_fovY;

	m_lastMouseX = 0.0f;
	m_lastMouseY = 0.0f;

	Update();
}

void Camera::Update()
{
	// Construct translation and rotation matrices...
	glm::mat4 posMat = glm::translate(glm::mat4(), m_position);
	glm::mat4 rotMat = glm::rotate(glm::mat4(), -m_eulerAngles.z, glm::vec3(0.0f, 0.0f, 1.0f));
	rotMat *= glm::rotate(rotMat, m_eulerAngles.y, glm::vec3(0.0f, 1.0f, 0.0f));
	rotMat *= glm::rotate(rotMat, m_eulerAngles.x, glm::vec3(1.0f, 0.0f, 0.0f));

	// The camera will rotate around a pivot at it's centre, so concatenate translation first and rotation second.
	m_matrix = posMat * rotMat;
}

void Camera::UpdateFreeCam(float fDeltaTime, Input* input, GLFWwindow* window) 
{
	float fNewMouseX = input->GetCursorX();
	float fNewMouseY = input->GetCursorY();

	// Look
	if (input->GetMouseButton(MOUSEBUTTON_RIGHT))
	{
		float fXDiff = fNewMouseX - m_lastMouseX;
		float fYDiff = fNewMouseY - m_lastMouseY;

		if(!m_bLooking) 
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			m_bLooking = true;
		}

		m_eulerAngles.y -= fXDiff * m_sensitivity * 0.001f;
		m_eulerAngles.x -= fYDiff * m_sensitivity * 0.001f;
	}
	else if(m_bLooking)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		m_bLooking = false;
	}

	m_lastMouseX = fNewMouseX;
	m_lastMouseY = fNewMouseY;

	// Strafe
	if (input->GetKey(GLFW_KEY_W))
	{
		glm::vec3 forwardVec = -m_matrix[2];

		m_position += forwardVec * m_moveSpeed * fDeltaTime;
	}
	if (input->GetKey(GLFW_KEY_A))
	{
		glm::vec3 leftVec = -m_matrix[0];

		m_position += leftVec * m_moveSpeed * fDeltaTime;
	}
	if (input->GetKey(GLFW_KEY_S))
	{
		glm::vec3 backVec = m_matrix[2];

		m_position += backVec * m_moveSpeed * fDeltaTime;
	}
	if (input->GetKey(GLFW_KEY_D))
	{
		glm::vec3 rightVec = m_matrix[0];

		m_position += rightVec * m_moveSpeed * fDeltaTime;
	}
	if (input->GetKey(GLFW_KEY_SPACE))
	{
		glm::vec3 upVec = m_matrix[1];

		m_position += upVec * m_moveSpeed * fDeltaTime;
	}
	if (input->GetKey(GLFW_KEY_LEFT_CONTROL))
	{
		glm::vec3 downVec = -m_matrix[1];

		m_position += downVec * m_moveSpeed * fDeltaTime;
	}

	Update();
}

glm::mat4 Camera::GetProjectionMatrix() const
{
	glm::mat4 axisCorrection; // Inverts the Y axis to match the OpenGL coordinate system.
	axisCorrection[1][1] = -1.0f;
	axisCorrection[2][2] = 1.0f;
	axisCorrection[3][3] = 1.0f;

	return axisCorrection * glm::perspectiveFov<float>(m_fovY, float(m_width), float(m_height), 0.1f, 1000.0f);
}

void Camera::SetWidth(uint32_t width)
{
	m_width = width;

	m_aspect = float(m_width) / float(m_height);
}

void Camera::SetHeight(uint32_t height)
{
	m_height = height;

	m_aspect = float(m_width) / float(m_height);
}
