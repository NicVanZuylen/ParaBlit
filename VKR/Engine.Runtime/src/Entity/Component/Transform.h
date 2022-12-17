#include "Entity/Entity.h"

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#pragma warning(pop)

namespace Eng
{
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

		void OnDestruction() override { ECDestroy(this); }

		void GetReflection(CLib::Reflector& outReflector) override { outReflector.Init(this); }

		inline void SetPosition(const glm::vec3& position) { m_translation = position; }
		inline void SetScale(const glm::vec3& scale) { m_scale = scale; }
		inline void Translate(const glm::vec3& translation) { m_translation += translation; }
		inline void ResetRotation() { m_quaternion = glm::identity<glm::quat>(); }
		void SetRotation(const glm::quat& quaternion) { m_quaternion = quaternion; }
		void RotateEulerX(float angle);
		void RotateEulerY(float angle);
		void RotateEulerZ(float angle);
		void Rotate(const glm::quat& quat) { m_quaternion *= quat; }
		inline void Scale(const glm::vec3& deltaScale) { m_scale += deltaScale; }

		inline glm::vec3 GetPosition() const { return m_translation; }
		inline glm::quat GetQuaternion() const { return m_quaternion; }
		inline glm::vec3 GetEulerAngles() const { return glm::degrees(glm::eulerAngles(m_quaternion)); }
		inline glm::vec3 GetScale() const { return m_scale; }

		void GetMatrix(glm::mat4& outMatrix) const;
		glm::mat4 GetMatrix() const;

	private:

		CLIB_REFLECTABLE(Transform,
			(glm::vec3) m_translation,
			(glm::quat) m_quaternion,
			(glm::vec3) m_scale
		)
	};
}