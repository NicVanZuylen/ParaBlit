#include "Entity/Entity.h"
#include "Engine.Math/Vector3.h"
#include "Engine.Math/Quaternion.h"
#include "Engine.Math/Matrix4.h"

namespace Eng
{
	using namespace Math;

	class Transform : public EntityComponent
	{
	public:

		Transform()
		{
			m_translation = glm::vec3(0.0f);
			m_quaternion = glm::identity<glm::quat>();
			m_scale = glm::vec3(1.0f);
		}

		~Transform() = default;

		static Transform* ECCreate()
		{
			return s_entityComponentStorage.Alloc<Transform>();
		}

		static void ECDestroy(Transform* component)
		{
			s_entityComponentStorage.Free(component);
		}

		void OnConstruction() override {};

		void OnDestruction() override { ECDestroy(this); }

		void GetReflection(CLib::Reflection::Reflector& outReflector) override { outReflector.Init(this); }

		inline void SetPosition(const Vector3f& position) { m_translation = position; }
		inline void SetScale(const Vector3f& scale) { m_scale = scale; }
		inline void Translate(const Vector3f& translation) { m_translation += translation; }
		inline void ResetRotation() { m_quaternion = glm::identity<glm::quat>(); }
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

		CLIB_REFLECTABLE(Transform,
			(Vector3f) m_translation,
			(Quaternion) m_quaternion,
			(Vector3f) m_scale
		)
	};
}