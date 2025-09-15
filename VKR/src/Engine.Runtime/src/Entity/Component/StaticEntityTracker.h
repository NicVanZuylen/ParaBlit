#pragma once
#include "StaticEntityTracker_generated.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "Entity/EntityBoundingVolumeHierarchy.h"

namespace Eng
{
	class RenderDefinition;
	class Transform;

	class StaticEntityTracker : public EntityComponent
	{
		REFLECTRON_CLASS()
	public:

		REFLECTRON_GENERATED_StaticEntityTracker()

		StaticEntityTracker() : EntityComponent(this)
		{
		}

		~StaticEntityTracker() = default;

		/*
		Description: Commit the entity at its current position with its current rendering properites to the scene as a static object.
		*/
		void CommitEntity();

		void UncommitEntity();

		/*
		Description: Update the committed entity's transform.
		*/
		void UpdateEntity();

		Bounds GetBoundingBox() { return m_entityObjectData.m_bounds; };

	private:

		void RetrieveEntityBounds(RenderDefinition* renderDef, const Transform* transform);

		EntityBoundingVolumeHierarchy::ObjectData m_entityObjectData;
	};
	CLIB_REFLECTABLE_CLASS(StaticEntityTracker)
}