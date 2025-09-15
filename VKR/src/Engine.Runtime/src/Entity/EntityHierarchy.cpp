#include "Entity/EntityHierarchy.h"
#include "Entity/Component/Transform.h"
#include "Entity/Component/RenderDefinition.h"
#include "Entity/Component/StaticEntityTracker.h"
#include "Entity/Component/DynamicEntityTracker.h"

#include "Engine.ParaBlit/ICommandContext.h"

#include "Resource/AssetStreamer.h"

namespace Eng
{
	EntityHierarchy::~EntityHierarchy()
	{
	}

	void EntityHierarchy::Init(Ctrl::IDataNode* entityData, CLib::Allocator* allocator, PB::IRenderer* renderer, AssetStreamer* streamer)
	{
		m_allocator = allocator;
		m_renderer = renderer;
		m_entityRoot = entityData;
		m_streamer = streamer;

		// Initialize bounding volume hierarchies.
		{
			BoundingVolumeHierarchy::CreateDesc bvhDesc{};

			bvhDesc.m_toleranceDistanceX = 0.5f;
			bvhDesc.m_toleranceDistanceZ = 0.5f;
			bvhDesc.m_toleranceDistanceY = 1.0f;

			bvhDesc.m_toleranceStepX = 0.5f;
			bvhDesc.m_toleranceStepZ = 0.5f;
			bvhDesc.m_toleranceStepY = 1.0f;

			m_renderHierarchy.Init(renderer, allocator, m_streamer, bvhDesc);
			m_entityBoundingVolumeHierarchy.Init(bvhDesc);
		}

		// Initialize dynamic draw pool
		DynamicDrawPool::Desc drawPoolDesc{};
		m_dynamicDrawPool.Init(drawPoolDesc, m_renderer, m_allocator);
		// Initialize static object renderer.
		m_staticObjectRenderer.Init(m_renderer, m_allocator);

		// Load entity data.
		InitDefaultResources();

		if (entityData != nullptr)
		{
			DataClass::InstantiateNodeTree(entityData);

			uint32_t entityCount = 0;
			auto entities = entityData->GetDataNode("Entity", entityCount);

			for (uint32_t i = 0; i < entityCount; ++i)
			{
				TObjectPtr<Entity> entity(entities[i]->GetSelfGUID());
				if (entity->GetComponent<StaticEntityTracker>() != nullptr)
				{
					entity->BindToHierarchy(this);
					CommitEntity(entity);
					m_entities.insert(entity);
				}
				else if (entity->GetComponent<DynamicEntityTracker>() != nullptr)
				{
					entity->BindToHierarchy(this);
					CommitEntity(entity);
					m_entities.insert(entity);
					m_dynamicEntities.insert(entity);
				}
			}
		}

		// With all entities and components constructed and linked, call OnInitialize for all entity components with everything now linked.
		for (auto entity : m_entities)
		{
			auto& components = entity->GetAllComponents();
			for (auto component : components)
			{
				component->OnInitialize();
			}
		}
	}

	void EntityHierarchy::SaveState(Ctrl::IDataFile* file, Ctrl::IDataNode* entityRoot)
	{
		DataClass::SaveNodeTree(entityRoot);
		file->WriteData();
	}

	void EntityHierarchy::Destroy()
	{
		m_entities.clear();
		m_entityBoundingVolumeHierarchy.Destroy();
		m_renderHierarchy.Destroy();
	}

	TObjectPtr<Entity> EntityHierarchy::CreateEntity(EEntityUpdateMethod updateMethod, bool initializeRendering, const char* meshName)
	{
		// Setup Entity.
		TObjectPtr<Entity> newEntity = CLib::Reflection::InstantiateClass<Entity>("Entity");
		newEntity->SaveToDataNode(m_entityRoot, m_entityRoot->AddDataNode("Entity", "Entity"));
		newEntity->BindToHierarchy(this);

		const char* finalMeshName = meshName != nullptr ? meshName : "Meshes/Objects/Stanford/Bunny";

		newEntity->AddComponent<Transform>()->SaveToDataNode(m_entityRoot, m_entityRoot->AddDataNode("Transform", "Transform"));

		if (initializeRendering == true)
		{
			newEntity->AddComponent<RenderDefinition>(finalMeshName, m_defaultMaterial, updateMethod)->SaveToDataNode(m_entityRoot, m_entityRoot->AddDataNode("RenderDefinition", "RenderDefinition"));;
		}

		if (updateMethod == EEntityUpdateMethod::STATIC)
		{
			newEntity->AddComponent<StaticEntityTracker>()->SaveToDataNode(m_entityRoot, m_entityRoot->AddDataNode("StaticEntityTracker", "StaticEntityTracker"));;
		}
		else
		{
			newEntity->AddComponent<DynamicEntityTracker>()->SaveToDataNode(m_entityRoot, m_entityRoot->AddDataNode("DynamicEntityTracker", "DynamicEntityTracker"));;
		}

		CommitEntity(newEntity);

		// Update hierarchy.
		m_entities.insert(newEntity);

		if (updateMethod == EEntityUpdateMethod::STATIC)
		{
			UpdateTrees();
		}
		else
		{
			m_dynamicEntities.insert(newEntity);
		}

		return newEntity;
	}

	void EntityHierarchy::DestroyEntity(Entity* entity)
	{
		TObjectPtr<StaticEntityTracker> tracker = entity->GetComponent<StaticEntityTracker>();
		if (tracker != nullptr)
		{
			tracker->UncommitEntity();
			auto renderDefinition = entity->GetComponent<RenderDefinition>();
			if (renderDefinition != nullptr)
			{
				m_staticObjectRenderer.RemoveObject(renderDefinition);
			}
			UpdateTrees();
		}
		else // Dynamic path
		{
			DynamicEntityTracker* dynamicTracker = entity->GetComponent<DynamicEntityTracker>();
			if (dynamicTracker != nullptr)
			{
				dynamicTracker->UncommitEntity();
			}
		}

		m_entities.erase(entity);
	}

	TObjectPtr<Entity> EntityHierarchy::FindEntity(const char* name)
	{
		for (auto entity : m_entities)
		{
			if (std::strcmp(entity->GetName(), name) == 0)
			{
				return entity;
			}
		}

		return TObjectPtr<Entity>();
	}

	void EntityHierarchy::CommitEntity(Entity* entity)
	{
		StaticEntityTracker* tracker = entity->GetComponent<StaticEntityTracker>();
		if (tracker != nullptr)
		{
			tracker->CommitEntity();

			auto renderDefinition = entity->GetComponent<RenderDefinition>();
			if (renderDefinition != nullptr)
			{
				m_staticObjectRenderer.AddObject(renderDefinition);
			}
		}
		else // Dynamic path
		{
			DynamicEntityTracker* dynamicTracker = entity->GetComponent<DynamicEntityTracker>();
			if (dynamicTracker != nullptr)
			{
				dynamicTracker->CommitEntity(&m_dynamicDrawPool);
			}
		}
	}

	void EntityHierarchy::UpdateTrees()
	{
		m_entityBoundingVolumeHierarchy.UpdateTree();
		m_staticObjectRenderer.ForceUpdate();
	}

	void EntityHierarchy::BakeTrees()
	{
		m_entityBoundingVolumeHierarchy.Build();
		m_staticObjectRenderer.ForceUpdate();
	}

	void EntityHierarchy::DynamicUpdate()
	{
		for (TObjectPtr<Entity> entity : m_dynamicEntities)
		{
			entity->GetComponent<DynamicEntityTracker>()->UpdateEntity();
		}
	}

	void EntityHierarchy::InitDefaultResources()
	{
		auto* defaultResourcesRoot = m_entityRoot->GetOrAddDataNode("DefaultResources");
		auto* defaultMaterialNode = defaultResourcesRoot->GetOrAddDataNode("Material");
		Material* defaultMat = CLib::Reflection::InstantiateClass<Material>("Material");
		if (defaultMaterialNode->GetSelfGUID() == Ctrl::nullGUID)
		{
			m_defaultMaterial = defaultMat;
			*m_defaultMaterial->GetReflection().GetFieldWithName<std::string>("m_name") = "mat_default";

			m_defaultMaterial->SaveToDataNode(defaultResourcesRoot, defaultMaterialNode);
		}
		else
		{
			defaultMat->FillFromDataNode(defaultMaterialNode);
			m_defaultMaterial = defaultMat;
		}
	}
}