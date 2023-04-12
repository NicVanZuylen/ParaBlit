#pragma once
#include "Entity/EntityBoundingVolumeHierarchy.h"

namespace Eng
{
	class EntityHierarchy;

	class StaticEntityTracker : public EntityComponent
	{
	public:

		StaticEntityTracker() = default;

		~StaticEntityTracker() = default;

		static StaticEntityTracker* ECCreate()
		{
			return s_entityComponentStorage.Alloc<StaticEntityTracker>();
		}

		static void ECDestroy(StaticEntityTracker* component)
		{
			s_entityComponentStorage.Free(component);
		}

		void OnConstruction() override {};

		void OnDestruction() override { ECDestroy(this); }

		/*
		Description: Commit the entity at its current position with its current rendering properites to the scene as a static object.
		*/
		void CommitEntity();

		void UncommitEntity();

		/*
		Description: Update the committed entity's transform.
		*/
		void UpdateEntity();

	private:

		EntityBoundingVolumeHierarchy::ObjectData m_entityObjectData;
	};
}