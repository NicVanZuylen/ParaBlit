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
	glm::mat4 posMat = glm::translate(glm::identity<glm::mat4>(), m_position);
	glm::mat4 rotMat = glm::rotate(glm::identity<glm::mat4>(), -m_eulerAngles.z, glm::vec3(0.0f, 0.0f, 1.0f));
	rotMat *= glm::rotate(rotMat, m_eulerAngles.y, glm::vec3(0.0f, 1.0f, 0.0f));
	rotMat *= glm::rotate(rotMat, m_eulerAngles.x, glm::vec3(1.0f, 0.0f, 0.0f));

	// The camera will rotate around a pivot at it's centre, so concatenate translation first and rotation second.
	m_matrix = posMat * rotMat;

	// Calculate frustrum
	const glm::vec3 nearCentre = Position() + (Forward() * ZNear());
	const glm::vec3 farCentre = Position() + (Forward() * ZFar());

	const float nearFarHeight = ZNear() * glm::tan(FovY() * 0.5f);
	const float nearFarWidth = nearFarHeight * Aspect();

	const float halfFarHeight = ZFar() * glm::tan(FovY() * 0.5f);
	const float halfFarWidth = halfFarHeight * Aspect();

	m_frustrum.m_nearTopLeft = nearCentre + (Up() * nearFarHeight) - (Right() * nearFarWidth);
	m_frustrum.m_nearTopRight = nearCentre + (Up() * nearFarHeight) + (Right() * nearFarWidth);
	m_frustrum.m_nearBottomLeft = nearCentre - (Up() * nearFarHeight) - (Right() * nearFarWidth);
	m_frustrum.m_nearBottomRight = nearCentre - (Up() * nearFarHeight) + (Right() * nearFarWidth);

	m_frustrum.m_farTopLeft = farCentre + (Up() * halfFarHeight) - (Right() * halfFarWidth);
	m_frustrum.m_farTopRight = farCentre + (Up() * halfFarHeight) + (Right() * halfFarWidth);
	m_frustrum.m_farBottomLeft = farCentre - (Up() * halfFarHeight) - (Right() * halfFarWidth);
	m_frustrum.m_farBottomRight = farCentre - (Up() * halfFarHeight) + (Right() * halfFarWidth);

	const glm::vec3 leftNormal = glm::normalize
	(
		glm::cross
		(
			m_frustrum.m_farBottomLeft - m_frustrum.m_nearBottomLeft,
			m_frustrum.m_farTopLeft - m_frustrum.m_farBottomLeft
		)
	);
	const glm::vec3 rightNormal = glm::normalize
	(
		glm::cross
		(
			m_frustrum.m_farTopRight - m_frustrum.m_nearTopRight,
			m_frustrum.m_farBottomRight - m_frustrum.m_farTopRight
		)
	);
	const glm::vec3 topNormal = glm::normalize
	(
		glm::cross
		(
			m_frustrum.m_farTopLeft - m_frustrum.m_nearTopLeft,
			m_frustrum.m_farTopRight - m_frustrum.m_farTopLeft
		)
	);
	const glm::vec3 bottomNormal = glm::normalize
	(
		glm::cross
		(
			m_frustrum.m_farBottomRight - m_frustrum.m_nearBottomRight,
			m_frustrum.m_farBottomLeft - m_frustrum.m_farBottomRight
		)
	);

	m_frustrum.m_left = { leftNormal, glm::dot(m_frustrum.m_farTopLeft, leftNormal) };
	m_frustrum.m_right = { rightNormal, glm::dot(m_frustrum.m_farBottomRight, rightNormal) };

	m_frustrum.m_top = { topNormal, glm::dot(m_frustrum.m_farTopLeft, topNormal) };
	m_frustrum.m_bottom = { bottomNormal, glm::dot(m_frustrum.m_farBottomLeft, bottomNormal) };

	const float projectedNear = glm::dot(Position() + (Forward() * ZNear()), -Forward());
	const float projectedFar = glm::dot(Position() + (Forward() * ZFar()), Forward());

	m_frustrum.m_near = glm::vec4(- Forward(), projectedNear);
	m_frustrum.m_far = { Forward(), projectedFar };

	glm::mat4 noTranslate = glm::translate(glm::mat4(), Position() * glm::vec3(2.0f, 0.0f, 2.0f));
	glm::mat4 trans = glm::rotate(noTranslate, glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));

	m_frustrum.m_nearTopLeft = trans * glm::vec4(m_frustrum.m_nearTopLeft, 1.0f);
	m_frustrum.m_nearTopRight = trans * glm::vec4(m_frustrum.m_nearTopRight, 1.0f);
	m_frustrum.m_nearBottomLeft = trans * glm::vec4(m_frustrum.m_nearBottomLeft, 1.0f);
	m_frustrum.m_nearBottomRight = trans * glm::vec4(m_frustrum.m_nearBottomRight, 1.0f);

	m_frustrum.m_farTopLeft = trans * glm::vec4(m_frustrum.m_farTopLeft, 1.0f);
	m_frustrum.m_farTopRight = trans * glm::vec4(m_frustrum.m_farTopRight, 1.0f);
	m_frustrum.m_farBottomLeft = trans * glm::vec4(m_frustrum.m_farBottomLeft, 1.0f);
	m_frustrum.m_farBottomRight = trans * glm::vec4(m_frustrum.m_farBottomRight, 1.0f);
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
