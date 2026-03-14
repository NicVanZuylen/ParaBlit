#pragma once
#include "DynamicEntityTracker_generated.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "Entity.h"
#include "Entity/Component/Transform.h"
#include "Entity/Component/RenderDefinition.h"
#include "WorldRender/Bounds.h"

namespace Eng
{
	class DynamicDrawPool;

	class DynamicEntityTracker : public EntityComponent
	{
		REFLECTRON_CLASS()
	public:

		REFLECTRON_GENERATED_DynamicEntityTracker()

		DynamicEntityTracker() : EntityComponent(this)
		{
		}

		~DynamicEntityTracker() = default;

		/*
		Description: Commit the entity at its current position with its current rendering properites to the scene as a dynamic object.
		*/
		void CommitEntity(DynamicDrawPool* drawPool);

		void UncommitEntity();

		/*
		Description: Update the committed entity's state.
		*/
		inline void UpdateEntity()
		{
			m_transform->GetMatrix(m_transformMatrix);

			if (m_renderDefinition != nullptr)
			{
				m_renderDefinition->SetPriorFrameTransform(m_transform->GetPosition(), m_transform->GetQuaternion(), m_transform->GetScale());
			}

			m_worldMeshBounds = m_meshBounds;
			m_worldMeshBounds.Transform(m_transformMatrix);
		}

		void GetTransformMatrix(Matrix4& outMatrix) { outMatrix = m_transformMatrix; };

		const Bounds& GetEntityWorldBounds() const { return m_worldMeshBounds; }

	private:

		TObjectPtr<Transform> m_transform;
		TObjectPtr<RenderDefinition> m_renderDefinition;

		Matrix4 m_transformMatrix = Matrix4::Identity();
		Bounds m_meshBounds = Bounds::Identity();
		Bounds m_worldMeshBounds = Bounds::Identity();
	};
	CLIB_REFLECTABLE_CLASS(DynamicEntityTracker)
}