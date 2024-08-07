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
		Matrix4 posMat = Matrix4::Identity().Translate(m_position);
		Matrix4 rotMat = Matrix4::Identity().Rotate(-m_eulerAngles.z, Vector3f(0.0f, 0.0f, 1.0f));
		rotMat *= rotMat.Rotated(m_eulerAngles.y * 0.5f, Vector3f(0.0f, 1.0f, 0.0f));
		rotMat *= rotMat.Rotated(m_eulerAngles.x, Vector3f(1.0f, 0.0f, 0.0f));

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
			Vector3f forwardVec = -m_matrix[2];

			m_position += forwardVec * m_moveSpeed * fDeltaTime;
		}
		if (input->GetKey(GLFW_KEY_A))
		{
			Vector3f leftVec = -m_matrix[0];

			m_position += leftVec * m_moveSpeed * fDeltaTime;
		}
		if (input->GetKey(GLFW_KEY_S))
		{
			Vector3f backVec = m_matrix[2];

			m_position += backVec * m_moveSpeed * fDeltaTime;
		}
		if (input->GetKey(GLFW_KEY_D))
		{
			Vector3f rightVec = m_matrix[0];

			m_position += rightVec * m_moveSpeed * fDeltaTime;
		}
		if (input->GetKey(GLFW_KEY_SPACE))
		{
			Vector3f upVec = m_matrix[1];

			m_position += upVec * m_moveSpeed * fDeltaTime;
		}
		if (input->GetKey(GLFW_KEY_LEFT_CONTROL))
		{
			Vector3f downVec = -m_matrix[1];

			m_position += downVec * m_moveSpeed * fDeltaTime;
		}

		Update();
	}

	Matrix4 Camera::GetProjectionMatrix() const
	{
		Matrix4 axisCorrection; // Inverts the Y axis to match the OpenGL coordinate system.
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

	Vector3f Camera::GetCursorNearPlaneWorldPosition(Vector2f cursorCoords)
	{
		float right = 1.0f - cursorCoords.x;
		float up = 1.0f - cursorCoords.y;

		Vector3f nearCornerDiff = m_frustrum.m_nearBottomRight - m_frustrum.m_nearTopLeft;
		Vector3f nearPos = m_frustrum.m_nearTopLeft + (Dot(nearCornerDiff, Right()) * Right() * right);
		nearPos += (Dot(nearCornerDiff, Up()) * Up() * up);

		return nearPos;
	}

	Vector3f Camera::GetCursorFarPlaneWorldPosition(Vector2f cursorCoords)
	{
		float right = 1.0f - cursorCoords.x;
		float up = 1.0f - cursorCoords.y;

		Vector3f farCornerDiff = m_frustrum.m_farBottomRight - m_frustrum.m_farTopLeft;
		Vector3f farPos = m_frustrum.m_farTopLeft + (Dot(farCornerDiff, Right()) * Right() * right);
		farPos += (Dot(farCornerDiff, Up()) * Up() * up);

		return farPos;
	}

	void Camera::GetFrustrumSection(CameraFrustrum& outFrustrum, float nearDistance, float farDistance) const
	{
		const Vector3f nearCentre = Position() + (Forward() * nearDistance);
		const Vector3f farCentre = Position() + (Forward() * farDistance);

		if (m_projectionType == EProjectionType::PERSPECTIVE)
		{
			const float nearFarHeight = nearDistance * Math::Tan(FovY() * 0.5f);
			const float nearFarWidth = nearFarHeight * Aspect();
			const float halfFarHeight = farDistance * Math::Tan(FovY() * 0.5f);
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

		const Vector3f leftNormal = Normalize
		(
			Cross
			(
				outFrustrum.m_farBottomLeft - outFrustrum.m_nearBottomLeft,
				outFrustrum.m_farTopLeft - outFrustrum.m_farBottomLeft
			)
		);
		const Vector3f rightNormal = Normalize
		(
			Cross
			(
				outFrustrum.m_farTopRight - outFrustrum.m_nearTopRight,
				outFrustrum.m_farBottomRight - outFrustrum.m_farTopRight
			)
		);
		const Vector3f topNormal = Normalize
		(
			Cross
			(
				outFrustrum.m_farTopLeft - outFrustrum.m_nearTopLeft,
				outFrustrum.m_farTopRight - outFrustrum.m_farTopLeft
			)
		);
		const Vector3f bottomNormal = Normalize
		(
			Cross
			(
				outFrustrum.m_farBottomRight - outFrustrum.m_nearBottomRight,
				outFrustrum.m_farBottomLeft - outFrustrum.m_farBottomRight
			)
		);

		if (m_projectionType == EProjectionType::PERSPECTIVE)
		{
			outFrustrum.m_left = { leftNormal, Dot(outFrustrum.m_farTopLeft, leftNormal) };
			outFrustrum.m_right = { rightNormal, Dot(outFrustrum.m_farBottomRight, rightNormal) };

			outFrustrum.m_top = { topNormal, Dot(outFrustrum.m_farTopLeft, topNormal) };
			outFrustrum.m_bottom = { bottomNormal, Dot(outFrustrum.m_farBottomLeft, bottomNormal) };

			const float projectedNear = Dot(Position() + (Forward() * nearDistance), -Forward());
			const float projectedFar = Dot(Position() - (Forward() * farDistance), Forward());

			outFrustrum.m_near = { -Forward(), projectedNear };
			outFrustrum.m_far = { Forward(), projectedFar };
		}
		else
		{
			outFrustrum.m_left = { -leftNormal, Dot(outFrustrum.m_farTopLeft, -leftNormal) };
			outFrustrum.m_right = { -rightNormal, Dot(outFrustrum.m_farBottomRight, -rightNormal) };

			outFrustrum.m_top = { -topNormal, Dot(outFrustrum.m_farTopLeft, -topNormal) };
			outFrustrum.m_bottom = { -bottomNormal, Dot(outFrustrum.m_farBottomLeft, -bottomNormal) };

			const float projectedNear = Dot(Position() + (Forward() * nearDistance), Forward());
			const float projectedFar = Dot(Position() - (Forward() * farDistance), Forward());

			outFrustrum.m_near = { Forward(), -projectedNear };
			outFrustrum.m_far = { Forward(), projectedFar };
		}

		Matrix4 noTranslate = Matrix4::Identity().Translate(Position() * Vector3f(2.0f, 2.0f, 2.0f));
		Matrix4 axisCorrection; // Inverts the Y axis to match the OpenGL coordinate system.
		axisCorrection[1][1] = -1.0f;
		axisCorrection[2][2] = 1.0f;
		axisCorrection[3][3] = 1.0f;

		Matrix4 trans = noTranslate.Rotated(Math::Pi<float>(), Vector3f(0.0f, 1.0f, 0.0f)) * axisCorrection;

		outFrustrum.m_nearTopLeft = trans * Vector4f(outFrustrum.m_nearTopLeft, 1.0f);
		outFrustrum.m_nearTopRight = trans * Vector4f(outFrustrum.m_nearTopRight, 1.0f);
		outFrustrum.m_nearBottomLeft = trans * Vector4f(outFrustrum.m_nearBottomLeft, 1.0f);
		outFrustrum.m_nearBottomRight = trans * Vector4f(outFrustrum.m_nearBottomRight, 1.0f);

		outFrustrum.m_farTopLeft = trans * Vector4f(outFrustrum.m_farTopLeft, 1.0f);
		outFrustrum.m_farTopRight = trans * Vector4f(outFrustrum.m_farTopRight, 1.0f);
		outFrustrum.m_farBottomLeft = trans * Vector4f(outFrustrum.m_farBottomLeft, 1.0f);
		outFrustrum.m_farBottomRight = trans * Vector4f(outFrustrum.m_farBottomRight, 1.0f);
	}

	void Camera::GetShadowCascadeFrustrum(CameraFrustrum& outFrustrum, Vector3f position, Vector3f forward, float leftBound, float rightBound, float bottomBound, float topBound, float nearDistance, float farDistance)
	{
		Vector3f right = -Normalize(Cross(forward, Vector3f(0.0f, 1.0f, 0.0f)));
		Vector3f up = Cross(forward, right);

		const Vector3f nearCentre = position + (forward * nearDistance);
		const Vector3f farCentre = position + (forward * farDistance);

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

		const Vector3f leftNormal = Normalize
		(
			Cross
			(
				outFrustrum.m_farBottomLeft - outFrustrum.m_nearBottomLeft,
				outFrustrum.m_farTopLeft - outFrustrum.m_farBottomLeft
			)
		);
		const Vector3f rightNormal = Normalize
		(
			Cross
			(
				outFrustrum.m_farTopRight - outFrustrum.m_nearTopRight,
				outFrustrum.m_farBottomRight - outFrustrum.m_farTopRight
			)
		);
		const Vector3f topNormal = Normalize
		(
			Cross
			(
				outFrustrum.m_farTopLeft - outFrustrum.m_nearTopLeft,
				outFrustrum.m_farTopRight - outFrustrum.m_farTopLeft
			)
		);
		const Vector3f bottomNormal = Normalize
		(
			Cross
			(
				outFrustrum.m_farBottomRight - outFrustrum.m_nearBottomRight,
				outFrustrum.m_farBottomLeft - outFrustrum.m_farBottomRight
			)
		);

		{
			outFrustrum.m_left = { -leftNormal, Dot(outFrustrum.m_farTopLeft, -leftNormal) };
			outFrustrum.m_right = { -rightNormal, Dot(outFrustrum.m_farBottomRight, -rightNormal) };

			outFrustrum.m_top = { -topNormal, Dot(outFrustrum.m_farTopLeft, -topNormal) };
			outFrustrum.m_bottom = { -bottomNormal, Dot(outFrustrum.m_farBottomLeft, -bottomNormal) };

			const float projectedNear = Dot(position + (forward * nearDistance), forward);
			const float projectedFar = Dot(position - (forward * farDistance), forward);

			outFrustrum.m_near = { forward, -projectedNear };
			outFrustrum.m_far = { forward, projectedFar };
		}

		Matrix4 noTranslate = Matrix4::Identity().Translate(position * Vector3f(2.0f, 2.0f, 2.0f));
		Matrix4 axisCorrection; // Inverts the Y axis to match the OpenGL coordinate system.
		axisCorrection[1][1] = -1.0f;
		axisCorrection[2][2] = 1.0f;
		axisCorrection[3][3] = 1.0f;

		Matrix4 trans = noTranslate.Rotated(Math::Pi<float>(), Vector3f(0.0f, 1.0f, 0.0f)) * axisCorrection;

		outFrustrum.m_nearTopLeft = trans * Vector4f(outFrustrum.m_nearTopLeft, 1.0f);
		outFrustrum.m_nearTopRight = trans * Vector4f(outFrustrum.m_nearTopRight, 1.0f);
		outFrustrum.m_nearBottomLeft = trans * Vector4f(outFrustrum.m_nearBottomLeft, 1.0f);
		outFrustrum.m_nearBottomRight = trans * Vector4f(outFrustrum.m_nearBottomRight, 1.0f);

		outFrustrum.m_farTopLeft = trans * Vector4f(outFrustrum.m_farTopLeft, 1.0f);
		outFrustrum.m_farTopRight = trans * Vector4f(outFrustrum.m_farTopRight, 1.0f);
		outFrustrum.m_farBottomLeft = trans * Vector4f(outFrustrum.m_farBottomLeft, 1.0f);
		outFrustrum.m_farBottomRight = trans * Vector4f(outFrustrum.m_farBottomRight, 1.0f);
	}

	void Camera::DrawFrustrum(DebugLinePass* linePass, const CameraFrustrum& frustrum, Vector3f frustrumColor)
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