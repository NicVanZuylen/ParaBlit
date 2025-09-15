#pragma once

#include "Entity/Entity.h"
#include "Transform_generated.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "Engine.Math/Vector3.h"
#include "Engine.Math/Quaternion.h"
#include "Engine.Math/Matrix4.h"
#include "Engine.Control/IDataClass.h"

namespace Eng
{
	using namespace Math;

	class Transform : public EntityComponent
	{
		REFLECTRON_CLASS()
	public:

		REFLECTRON_GENERATED_Transform()

		Transform() : EntityComponent(this)
		{
		}

		~Transform() = default;

		inline void SetPosition(const Vector3f& position) { m_translation = position; }
		inline void SetScale(const Vector3f& scale) { m_scale = scale; }
		inline void Translate(const Vector3f& translation) { m_translation += translation; }
		inline void ResetRotation() { m_quaternion = Quaternion::Identity(); }
		void SetRotation(const Quaternion& quaternion) { m_quaternion = quaternion; }
		void RotateEulerX(float angle);
		void RotateEulerY(float angle);
		void RotateEulerZ(float angle);
		void Rotate(const Quaternion& quat) { m_quaternion *= quat; }
		inline void Scale(const Vector3f& deltaScale) { m_scale += deltaScale; }

		inline Vector3f GetPosition() const { return m_translation; }
		inline Quaternion GetQuaternion() const { return m_quaternion; }
		inline Vector3f GetEulerAngles() const { return m_quaternion.ToEuler(); }
		inline Vector3f GetScale() const { return m_scale; }

		void GetMatrix(Matrix4& outMatrix) const;
		Matrix4 GetMatrix() const;

	private:

		REFLECTRON_FIELD()
		Vector3f m_translation = Vector3f(0.0f);
		REFLECTRON_FIELD()
		Quaternion m_quaternion = Quaternion::Identity();
		REFLECTRON_FIELD()
		Vector3f m_scale = Vector3f(1.0f);
	};
	CLIB_REFLECTABLE_CLASS(Transform)
}