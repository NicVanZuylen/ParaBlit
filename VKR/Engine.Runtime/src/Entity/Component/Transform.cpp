#include "Transform.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

namespace Eng
{
	void Transform::RotateEulerX(float angle)
	{
		m_quaternion = glm::rotate(m_quaternion, glm::radians(angle), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	void Transform::RotateEulerY(float angle)
	{
		m_quaternion = glm::rotate(m_quaternion, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	void Transform::RotateEulerZ(float angle)
	{
		m_quaternion = glm::rotate(m_quaternion, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));
	}

	void Transform::GetMatrix(glm::mat4& outMatrix) const
	{
		outMatrix = glm::translate(glm::identity<glm::mat4>(), glm::vec3(m_translation));
		outMatrix *= glm::toMat4(m_quaternion);
		outMatrix = glm::scale(outMatrix, glm::vec3(m_scale));
	}

	glm::mat4 Transform::GetMatrix() const
	{
		glm::mat4 mat;
		GetMatrix(mat);

		return mat;
	}
}