#pragma once
#include "DynamicEntityTracker_generated.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "Entity.h"

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
		void UpdateEntity();

	private:

	};
	CLIB_REFLECTABLE_CLASS(DynamicEntityTracker)
}