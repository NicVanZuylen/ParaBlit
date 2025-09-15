#pragma once
#include "CLib/FixedBlockAllocator.h"
#include "Entity/EntityBoundingVolumeHierarchy.h"
#include "WorldRender/RenderBoundingVolumeHierarchy.h"
#include "WorldRender/DynamicDrawPool.h"
#include "WorldRender/StaticObjectRenderer.h"
#include "Engine.Math/Vector4.h"

#include <unordered_set>


namespace Eng
{
	using namespace Math;

	class AssetStreamer;

	class EntityHierarchy
	{
	public:

		EntityHierarchy() = default;
		~EntityHierarchy();

		void Init(Ctrl::IDataNode* entityData, CLib::Allocator* allocator, PB::IRenderer* renderer, AssetStreamer* streamer);
		void SaveState(Ctrl::IDataFile* file, Ctrl::IDataNode* entityRoot);
		void Destroy();

		TObjectPtr<Entity> CreateEntity(EEntityUpdateMethod updateMethod = EEntityUpdateMethod::STATIC, bool initializeRendering = false, const char* meshName = nullptr);
		void DestroyEntity(Entity* entity);

		TObjectPtr<Entity> FindEntity(const char* name);

		inline DynamicDrawPool& GetDynamicDrawPool() { return m_dynamicDrawPool; }
		inline StaticObjectRenderer& GetStaticObjectRenderer() { return m_staticObjectRenderer; }
		inline RenderBoundingVolumeHierarchy& GetRenderHierarchy() { return m_renderHierarchy; }
		inline EntityBoundingVolumeHierarchy& GetEntityBoundingVolumeHierarchy() { return m_entityBoundingVolumeHierarchy; }

		inline PB::IRenderer* GetRenderer() { return m_renderer; }
		inline AssetStreamer* GetAssetStreamer() { return m_streamer; }

		/*
		Description: Commit the entity at its current position with its current rendering properites to the scene as a static object.
		*/
		void CommitEntity(Entity* entity);

		void UpdateTrees();

		void BakeTrees();

		void DynamicUpdate();

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

		std::unordered_set<TObjectPtr<Entity>, TObjectPtr<Entity>::Hash> m_entities;
		std::unordered_set<TObjectPtr<Entity>, TObjectPtr<Entity>::Hash> m_dynamicEntities;

		// Default resources
		TObjectPtr<Material> m_defaultMaterial;
	};
}