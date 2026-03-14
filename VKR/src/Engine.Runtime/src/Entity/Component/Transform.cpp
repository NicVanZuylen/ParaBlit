#include "Transform.h"
#include "Engine.Control/IDataClass.h"
#include "EntityHierarchy.h"
#include "StaticEntityTracker.h"
#include <cstdio>

namespace Eng
{
	void Transform::RotateEulerX(float angle)
	{
		m_quaternion.Rotate(angle, Vector3f(1.0f, 0.0f, 0.0f));
	}

	void Transform::RotateEulerY(float angle)
	{
		m_quaternion.Rotate(angle, Vector3f(0.0f, 1.0f, 0.0f));
	}

	void Transform::RotateEulerZ(float angle)
	{
		m_quaternion.Rotate(angle, Vector3f(0.0f, 0.0f, 1.0f));
	}

	void Transform::GetMatrix(Matrix4& outMatrix) const
	{
		outMatrix.ToIdentity();
		outMatrix.Translate(m_translation);
		outMatrix *= m_quaternion.ToMatrix4();
		outMatrix.Scale(m_scale);
	}

	Matrix4 Transform::GetMatrix() const
	{
		Matrix4 mat;
		GetMatrix(mat);

		return mat;
	}

	void Transform::OnFieldChanged(const ReflectronFieldData& field)
	{
		auto staticTracker = m_host->GetComponent<StaticEntityTracker>();
		if(staticTracker != nullptr)
		{
			// Trigger host hot reload...
			printf("Static entity %s's [%p] transform has been modified, triggering hot-reload.\n", m_host->GetName(), m_host);

			staticTracker->UpdateEntity();
			m_host->GetHierarchy()->UpdateTrees();
		}
	}
}