#pragma once

#include "Entity/Entity.h"
#include "EntitySpin_generated.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "Engine.Math/Vector3.h"
#include "Engine.Math/Quaternion.h"
#include "Engine.Math/Matrix4.h"
#include "Engine.Control/IDataClass.h"

namespace Eng
{
	using namespace Math;

	class EntitySpin : public EntityComponent
	{
		REFLECTRON_CLASS()
	public:

		REFLECTRON_GENERATED_EntitySpin()

		EntitySpin() : EntityComponent(this)
		{
		}

		~EntitySpin() = default;

		void OnInitialize() override;

		virtual void OnSimUpdate() override final;

	private:

		REFLECTRON_FIELD()
		bool m_translate = true;

		REFLECTRON_FIELD()
		bool m_spin = true;

		REFLECTRON_FIELD()
		bool m_scale = true;

		Vector3f m_initialPosition;
		float m_sinX = 0.0f;
	};
	CLIB_REFLECTABLE_CLASS(EntitySpin)
}