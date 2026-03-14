#pragma once
#include "EntityHierarchy_generated.h"
#include "Engine.Control/IDataClass.h"
#include "CLib/Reflection.h"
#include "CLib/FixedBlockAllocator.h"
#include "Entity/EntityBoundingVolumeHierarchy.h"
#include "Entity/DynamicEntitySpatialHashTable.h"
#include "WorldRender/RenderBoundingVolumeHierarchy.h"
#include "WorldRender/DynamicDrawPool.h"
#include "WorldRender/StaticObjectRenderer.h"
#include "Engine.Math/Vector4.h"

#include <unordered_set>


namespace Eng
{
	using namespace Math;

	class AssetStreamer;

	class EntityHierarchy : public Ctrl::DataClass
	{
		REFLECTRON_CLASS()

	public:

		REFLECTRON_GENERATED_EntityHierarchy()

		using EntityMap = std::unordered_set<TObjectPtr<Entity>, TObjectPtr<Entity>::Hash>;

		EntityHierarchy() : Ctrl::DataClass(this)
		{
		}
		~EntityHierarchy();

		void Init(Ctrl::IDataNode* entityData, CLib::Allocator* allocator, PB::IRenderer* renderer, AssetStreamer* streamer);
		void SaveState(Ctrl::IDataFile* file, Ctrl::IDataNode* entityRoot);
		void Destroy();

		TObjectPtr<Entity> CreateEntity(EEntityUpdateMethod updateMethod = EEntityUpdateMethod::STATIC, bool initializeRendering = false);
		void UncommitEntity(Entity* entity);
		void DestroyEntity(Entity* entity);

		TObjectPtr<Entity> FindEntity(const char* name);

		inline DynamicDrawPool& GetDynamicDrawPool() { return m_dynamicDrawPool; }
		inline StaticObjectRenderer& GetStaticObjectRenderer() { return m_staticObjectRenderer; }
		inline RenderBoundingVolumeHierarchy& GetRenderHierarchy() { return m_renderHierarchy; }
		inline EntityBoundingVolumeHierarchy& GetEntityBoundingVolumeHierarchy() { return m_entityBoundingVolumeHierarchy; }
		inline DynamicEntitySpatialHashTable& GetEntitySpatialHashTable() { return m_entitySpatialHashTable; }
		inline const EntityMap& GetAllEntities() const { return m_entityMap; }

		inline PB::IRenderer* GetRenderer() { return m_renderer; }
		inline AssetStreamer* GetAssetStreamer() { return m_streamer; }

		/*
		Description: Commit the entity at its current position with its current rendering properites to the scene as a static object.
		*/
		void CommitEntity(Entity* entity);

		void UpdateTrees();

		void BakeTrees();

		void SimUpdate();

		void RenderUpdate(const float& interpT);

		void SetSimulationEnable(bool enableSimulation) { m_simulationEnabled = enableSimulation; }

		const TObjectPtr<Material>& GetDefaultMaterial() const { return m_defaultMaterial; }

	private:

		void InitDefaultResources();

		CLib::Allocator* m_allocator = nullptr;
		PB::IRenderer* m_renderer = nullptr;
		Ctrl::IDataNode* m_entityRoot = nullptr;
		AssetStreamer* m_streamer = nullptr;
		DynamicDrawPool m_dynamicDrawPool;
		StaticObjectRenderer m_staticObjectRenderer;
		RenderBoundingVolumeHierarchy m_renderHierarchy;
		EntityBoundingVolumeHierarchy m_entityBoundingVolumeHierarchy;
		bool m_simulationEnabled = false;

		DynamicEntitySpatialHashTable m_entitySpatialHashTable;
		std::vector<TObjectPtr<Entity>> m_dynamicEntities;


		EntityMap m_entityMap; // All live entity instances.

		// Default resources
		TObjectPtr<Material> m_defaultMaterial;

		REFLECTRON_FIELD()
		TObjectPtrArray<Entity> m_entities; // Used only for fetching entities.
	};
	CLIB_REFLECTABLE_CLASS(EntityHierarchy)
}