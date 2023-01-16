#include "WorldRender/Camera.h"
#include "Utility/Input.h"
#include "RenderGraphPasses/DebugLinePass.h"
#include "GLFW/glfw3.h"

#include <iostream>

namespace Eng
{

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
		m_projectionType = desc.m_projectionType;
		m_width = desc.m_width;
		m_height = desc.m_height;
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
		rotMat *= glm::rotate(rotMat, m_eulerAngles.y * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
		rotMat *= glm::rotate(rotMat, m_eulerAngles.x, glm::vec3(1.0f, 0.0f, 0.0f));

		// The camera will rotate around a pivot at it's centre, so concatenate translation first and rotation second.
		m_matrix = posMat * rotMat;

		// Calculate frustrum
		GetFrustrumSection(m_frustrum, ZNear(), ZFar());
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

			if (!m_bLooking)
			{
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				m_bLooking = true;
			}

			m_eulerAngles.y -= fXDiff * m_sensitivity * 0.001f;
			m_eulerAngles.x -= fYDiff * m_sensitivity * 0.001f;
		}
		else if (m_bLooking)
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

		if (m_projectionType == EProjectionType::PERSPECTIVE)
		{
			return axisCorrection * glm::perspectiveFov<float>(m_fovY, m_width, m_height, m_zNear, m_zFar);
		}
		else if (m_projectionType == EProjectionType::ORTHOGRAPHIC)
		{
			float halfWidth = m_width * 0.5f;
			float halfHeight = m_height * 0.5f;
			return axisCorrection * glm::orthoZO(-halfWidth, halfWidth, -halfHeight, halfHeight, m_zNear, m_zFar);
		}

		return axisCorrection * glm::perspectiveFov<float>(m_fovY, m_width, m_height, m_zNear, m_zFar);
	}

	void Camera::GetFrustrumSection(CameraFrustrum& outFrustrum, float nearDistance, float farDistance) const
	{
		const glm::vec3 nearCentre = Position() + (Forward() * nearDistance);
		const glm::vec3 farCentre = Position() + (Forward() * farDistance);

		if (m_projectionType == EProjectionType::PERSPECTIVE)
		{
			const float nearFarHeight = nearDistance * glm::tan(FovY() * 0.5f);
			const float nearFarWidth = nearFarHeight * Aspect();
			const float halfFarHeight = farDistance * glm::tan(FovY() * 0.5f);
			const float halfFarWidth = halfFarHeight * Aspect();

			outFrustrum.m_nearTopLeft = nearCentre + (Up() * nearFarHeight) - (Right() * nearFarWidth);
			outFrustrum.m_nearTopRight = nearCentre + (Up() * nearFarHeight) + (Right() * nearFarWidth);
			outFrustrum.m_nearBottomLeft = nearCentre - (Up() * nearFarHeight) - (Right() * nearFarWidth);
			outFrustrum.m_nearBottomRight = nearCentre - (Up() * nearFarHeight) + (Right() * nearFarWidth);

			outFrustrum.m_farTopLeft = farCentre + (Up() * halfFarHeight) - (Right() * halfFarWidth);
			outFrustrum.m_farTopRight = farCentre + (Up() * halfFarHeight) + (Right() * halfFarWidth);
			outFrustrum.m_farBottomLeft = farCentre - (Up() * halfFarHeight) - (Right() * halfFarWidth);
			outFrustrum.m_farBottomRight = farCentre - (Up() * halfFarHeight) + (Right() * halfFarWidth);
		}
		else
		{
			const float halfWidth = m_width * 0.5f;
			const float halfHeight = m_height * 0.5f;

			outFrustrum.m_nearTopLeft = nearCentre + (Up() * halfHeight) - (Right() * halfWidth);
			outFrustrum.m_nearTopRight = nearCentre + (Up() * halfHeight) + (Right() * halfWidth);
			outFrustrum.m_nearBottomLeft = nearCentre - (Up() * halfHeight) - (Right() * halfWidth);
			outFrustrum.m_nearBottomRight = nearCentre - (Up() * halfHeight) + (Right() * halfWidth);

			outFrustrum.m_farTopLeft = farCentre + (Up() * halfHeight) - (Right() * halfWidth);
			outFrustrum.m_farTopRight = farCentre + (Up() * halfHeight) + (Right() * halfWidth);
			outFrustrum.m_farBottomLeft = farCentre - (Up() * halfHeight) - (Right() * halfWidth);
			outFrustrum.m_farBottomRight = farCentre - (Up() * halfHeight) + (Right() * halfWidth);
		}

		const glm::vec3 leftNormal = glm::normalize
		(
			glm::cross
			(
				outFrustrum.m_farBottomLeft - outFrustrum.m_nearBottomLeft,
				outFrustrum.m_farTopLeft - outFrustrum.m_farBottomLeft
			)
		);
		const glm::vec3 rightNormal = glm::normalize
		(
			glm::cross
			(
				outFrustrum.m_farTopRight - outFrustrum.m_nearTopRight,
				outFrustrum.m_farBottomRight - outFrustrum.m_farTopRight
			)
		);
		const glm::vec3 topNormal = glm::normalize
		(
			glm::cross
			(
				outFrustrum.m_farTopLeft - outFrustrum.m_nearTopLeft,
				outFrustrum.m_farTopRight - outFrustrum.m_farTopLeft
			)
		);
		const glm::vec3 bottomNormal = glm::normalize
		(
			glm::cross
			(
				outFrustrum.m_farBottomRight - outFrustrum.m_nearBottomRight,
				outFrustrum.m_farBottomLeft - outFrustrum.m_farBottomRight
			)
		);

		if (m_projectionType == EProjectionType::PERSPECTIVE)
		{
			outFrustrum.m_left = { leftNormal, glm::dot(outFrustrum.m_farTopLeft, leftNormal) };
			outFrustrum.m_right = { rightNormal, glm::dot(outFrustrum.m_farBottomRight, rightNormal) };

			outFrustrum.m_top = { topNormal, glm::dot(outFrustrum.m_farTopLeft, topNormal) };
			outFrustrum.m_bottom = { bottomNormal, glm::dot(outFrustrum.m_farBottomLeft, bottomNormal) };

			const float projectedNear = glm::dot(Position() + (Forward() * nearDistance), -Forward());
			const float projectedFar = glm::dot(Position() - (Forward() * farDistance), Forward());

			outFrustrum.m_near = { -Forward(), projectedNear };
			outFrustrum.m_far = { Forward(), projectedFar };
		}
		else
		{
			outFrustrum.m_left = { -leftNormal, glm::dot(outFrustrum.m_farTopLeft, -leftNormal) };
			outFrustrum.m_right = { -rightNormal, glm::dot(outFrustrum.m_farBottomRight, -rightNormal) };

			outFrustrum.m_top = { -topNormal, glm::dot(outFrustrum.m_farTopLeft, -topNormal) };
			outFrustrum.m_bottom = { -bottomNormal, glm::dot(outFrustrum.m_farBottomLeft, -bottomNormal) };

			const float projectedNear = glm::dot(Position() + (Forward() * nearDistance), Forward());
			const float projectedFar = glm::dot(Position() - (Forward() * farDistance), Forward());

			outFrustrum.m_near = { Forward(), -projectedNear };
			outFrustrum.m_far = { Forward(), projectedFar };
		}

		glm::mat4 noTranslate = glm::translate(glm::mat4(), Position() * glm::vec3(2.0f, 2.0f, 2.0f));
		glm::mat4 axisCorrection; // Inverts the Y axis to match the OpenGL coordinate system.
		axisCorrection[1][1] = -1.0f;
		axisCorrection[2][2] = 1.0f;
		axisCorrection[3][3] = 1.0f;

		glm::mat4 trans = glm::rotate(noTranslate, glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)) * axisCorrection;

		outFrustrum.m_nearTopLeft = trans * glm::vec4(outFrustrum.m_nearTopLeft, 1.0f);
		outFrustrum.m_nearTopRight = trans * glm::vec4(outFrustrum.m_nearTopRight, 1.0f);
		outFrustrum.m_nearBottomLeft = trans * glm::vec4(outFrustrum.m_nearBottomLeft, 1.0f);
		outFrustrum.m_nearBottomRight = trans * glm::vec4(outFrustrum.m_nearBottomRight, 1.0f);

		outFrustrum.m_farTopLeft = trans * glm::vec4(outFrustrum.m_farTopLeft, 1.0f);
		outFrustrum.m_farTopRight = trans * glm::vec4(outFrustrum.m_farTopRight, 1.0f);
		outFrustrum.m_farBottomLeft = trans * glm::vec4(outFrustrum.m_farBottomLeft, 1.0f);
		outFrustrum.m_farBottomRight = trans * glm::vec4(outFrustrum.m_farBottomRight, 1.0f);
	}

	void Camera::GetShadowCascadeFrustrum(CameraFrustrum& outFrustrum, glm::vec3 position, glm::vec3 forward, float leftBound, float rightBound, float bottomBound, float topBound, float nearDistance, float farDistance)
	{
		glm::vec3 right = -glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::vec3 up = glm::cross(forward, right);

		const glm::vec3 nearCentre = position + (forward * nearDistance);
		const glm::vec3 farCentre = position + (forward * farDistance);

		{
			outFrustrum.m_nearTopLeft = nearCentre + (up * topBound) + (right * leftBound);
			outFrustrum.m_nearTopRight = nearCentre + (up * topBound) + (right * rightBound);
			outFrustrum.m_nearBottomLeft = nearCentre + (up * bottomBound) + (right * leftBound);
			outFrustrum.m_nearBottomRight = nearCentre + (up * bottomBound) + (right * rightBound);

			outFrustrum.m_farTopLeft = farCentre + (up * topBound) + (right * leftBound);
			outFrustrum.m_farTopRight = farCentre + (up * topBound) + (right * rightBound);
			outFrustrum.m_farBottomLeft = farCentre + (up * bottomBound) + (right * leftBound);
			outFrustrum.m_farBottomRight = farCentre + (up * bottomBound) + (right * rightBound);
		}

		const glm::vec3 leftNormal = glm::normalize
		(
			glm::cross
			(
				outFrustrum.m_farBottomLeft - outFrustrum.m_nearBottomLeft,
				outFrustrum.m_farTopLeft - outFrustrum.m_farBottomLeft
			)
		);
		const glm::vec3 rightNormal = glm::normalize
		(
			glm::cross
			(
				outFrustrum.m_farTopRight - outFrustrum.m_nearTopRight,
				outFrustrum.m_farBottomRight - outFrustrum.m_farTopRight
			)
		);
		const glm::vec3 topNormal = glm::normalize
		(
			glm::cross
			(
				outFrustrum.m_farTopLeft - outFrustrum.m_nearTopLeft,
				outFrustrum.m_farTopRight - outFrustrum.m_farTopLeft
			)
		);
		const glm::vec3 bottomNormal = glm::normalize
		(
			glm::cross
			(
				outFrustrum.m_farBottomRight - outFrustrum.m_nearBottomRight,
				outFrustrum.m_farBottomLeft - outFrustrum.m_farBottomRight
			)
		);

		{
			outFrustrum.m_left = { -leftNormal, glm::dot(outFrustrum.m_farTopLeft, -leftNormal) };
			outFrustrum.m_right = { -rightNormal, glm::dot(outFrustrum.m_farBottomRight, -rightNormal) };

			outFrustrum.m_top = { -topNormal, glm::dot(outFrustrum.m_farTopLeft, -topNormal) };
			outFrustrum.m_bottom = { -bottomNormal, glm::dot(outFrustrum.m_farBottomLeft, -bottomNormal) };

			const float projectedNear = glm::dot(position + (forward * nearDistance), forward);
			const float projectedFar = glm::dot(position - (forward * farDistance), forward);

			outFrustrum.m_near = { forward, -projectedNear };
			outFrustrum.m_far = { forward, projectedFar };
		}

		glm::mat4 noTranslate = glm::translate(glm::mat4(), position * glm::vec3(2.0f, 2.0f, 2.0f));
		glm::mat4 axisCorrection; // Inverts the Y axis to match the OpenGL coordinate system.
		axisCorrection[1][1] = -1.0f;
		axisCorrection[2][2] = 1.0f;
		axisCorrection[3][3] = 1.0f;

		glm::mat4 trans = glm::rotate(noTranslate, glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)) * axisCorrection;

		outFrustrum.m_nearTopLeft = trans * glm::vec4(outFrustrum.m_nearTopLeft, 1.0f);
		outFrustrum.m_nearTopRight = trans * glm::vec4(outFrustrum.m_nearTopRight, 1.0f);
		outFrustrum.m_nearBottomLeft = trans * glm::vec4(outFrustrum.m_nearBottomLeft, 1.0f);
		outFrustrum.m_nearBottomRight = trans * glm::vec4(outFrustrum.m_nearBottomRight, 1.0f);

		outFrustrum.m_farTopLeft = trans * glm::vec4(outFrustrum.m_farTopLeft, 1.0f);
		outFrustrum.m_farTopRight = trans * glm::vec4(outFrustrum.m_farTopRight, 1.0f);
		outFrustrum.m_farBottomLeft = trans * glm::vec4(outFrustrum.m_farBottomLeft, 1.0f);
		outFrustrum.m_farBottomRight = trans * glm::vec4(outFrustrum.m_farBottomRight, 1.0f);
	}

	void Camera::DrawFrustrum(DebugLinePass* linePass, const CameraFrustrum& frustrum, glm::vec3 frustrumColor)
	{
		linePass->DrawLine(frustrum.m_nearTopLeft, frustrum.m_nearTopRight, frustrumColor);
		linePass->DrawLine(frustrum.m_nearTopLeft, frustrum.m_nearBottomLeft, frustrumColor);
		linePass->DrawLine(frustrum.m_nearBottomLeft, frustrum.m_nearBottomRight, frustrumColor);
		linePass->DrawLine(frustrum.m_nearBottomRight, frustrum.m_nearTopRight, frustrumColor);

		linePass->DrawLine(frustrum.m_farTopLeft, frustrum.m_farTopRight, frustrumColor);
		linePass->DrawLine(frustrum.m_farTopLeft, frustrum.m_farBottomLeft, frustrumColor);
		linePass->DrawLine(frustrum.m_farBottomLeft, frustrum.m_farBottomRight, frustrumColor);
		linePass->DrawLine(frustrum.m_farBottomRight, frustrum.m_farTopRight, frustrumColor);

		linePass->DrawLine(frustrum.m_nearTopLeft, frustrum.m_farTopLeft, frustrumColor);
		linePass->DrawLine(frustrum.m_nearBottomLeft, frustrum.m_farBottomLeft, frustrumColor);
		linePass->DrawLine(frustrum.m_nearTopRight, frustrum.m_farTopRight, frustrumColor);
		linePass->DrawLine(frustrum.m_nearBottomRight, frustrum.m_farBottomRight, frustrumColor);
	}

	void Camera::SetWidth(float width)
	{
		m_width = width;
		m_aspect = m_width / m_height;
	}

	void Camera::SetHeight(float height)
	{
		m_height = height;
		m_aspect = m_width / m_height;
	}

};