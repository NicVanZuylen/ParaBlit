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

		m_entityMap.insert(m_entities.begin(), m_entities.end());
		m_entities.Clear();

		for (TObjectPtr<Entity> entity : m_entityMap)
		{
			if (entity->GetComponent<StaticEntityTracker>() != nullptr)
			{
				entity->BindToHierarchy(this);
				CommitEntity(entity);
			}
			else if (entity->GetComponent<DynamicEntityTracker>() != nullptr)
			{
				entity->BindToHierarchy(this);
				CommitEntity(entity);
				m_dynamicEntities.push_back(entity);
			}
		}


		// With all entities and components constructed and linked, call OnInitialize for all entity components with everything now linked.
		for (TObjectPtr<Entity> entity : m_entityMap)
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
		for (TObjectPtr<Entity> entity : m_entityMap)
		{
			m_entities.PushBack(entity);
		}

		DataClass::SaveNodeTree(entityRoot);
		file->WriteData();

		m_entities.Clear();
	}

	void EntityHierarchy::Destroy()
	{
		m_entityMap.clear();
		m_entityBoundingVolumeHierarchy.Destroy();
		m_renderHierarchy.Destroy();
	}

	TObjectPtr<Entity> EntityHierarchy::CreateEntity(EEntityUpdateMethod updateMethod, bool initializeRendering)
	{
		// Setup Entity.
		TObjectPtr<Entity> newEntity = CLib::Reflection::InstantiateClass<Entity>("Entity");
		newEntity->SaveToDataNode(m_entityRoot->AddDataNode("Entity", "Entity"));
		newEntity->BindToHierarchy(this);

		newEntity->AddComponent<Transform>()->SaveToDataNode(m_entityRoot->AddDataNode("Transform", "Transform"));

		if (initializeRendering == true)
		{
			newEntity->AddComponent<RenderDefinition>(m_defaultMaterial, updateMethod)->SaveToDataNode(m_entityRoot->AddDataNode("RenderDefinition", "RenderDefinition"));
		}

		if (updateMethod == EEntityUpdateMethod::STATIC)
		{
			newEntity->AddComponent<StaticEntityTracker>()->SaveToDataNode(m_entityRoot->AddDataNode("StaticEntityTracker", "StaticEntityTracker"));
		}
		else
		{
			newEntity->AddComponent<DynamicEntityTracker>()->SaveToDataNode(m_entityRoot->AddDataNode("DynamicEntityTracker", "DynamicEntityTracker"));
		}

		CommitEntity(newEntity);

		// Update hierarchy.
		m_entityMap.insert(newEntity);

		if (updateMethod == EEntityUpdateMethod::STATIC)
		{
			UpdateTrees();
		}
		else
		{
			m_dynamicEntities.push_back(newEntity);
		}

		return newEntity;
	}

	void EntityHierarchy::UncommitEntity(Entity* entity)
	{
		StaticEntityTracker* tracker = entity->GetComponent<StaticEntityTracker>();
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
	}

	void EntityHierarchy::DestroyEntity(Entity* entity)
	{
		UncommitEntity(entity);

		DynamicEntityTracker* dynamicTracker = entity->GetComponent<DynamicEntityTracker>();
		if(dynamicTracker != nullptr)
		{
			for (auto it = m_dynamicEntities.begin(); it != m_dynamicEntities.end(); ++it)
			{
				if (it->GetPtr() == entity)
				{
					m_dynamicEntities.erase(it);
					break;
				}
			}
		}

		m_entityMap.erase(entity);
	}

	TObjectPtr<Entity> EntityHierarchy::FindEntity(const char* name)
	{
		for (auto entity : m_entityMap)
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

	void EntityHierarchy::SimUpdate()
	{
		// ------------------------------------------------------------------------------------------------------------------
		// Simulation update
		if (m_simulationEnabled == true)
		{
			for (TObjectPtr<Entity>& entity : m_dynamicEntities)
			{
				auto& components = entity->GetAllComponents();
				for (EntityComponent* component : components)
				{
					component->OnSimUpdate();
				}
			}
		}
		// ------------------------------------------------------------------------------------------------------------------

		// ------------------------------------------------------------------------------------------------------------------
		// Spatial hash table update
		m_entitySpatialHashTable.Reset();
		for (Entity* entity : m_dynamicEntities)
		{
			DynamicEntityTracker* tracker = entity->GetComponentPtr<DynamicEntityTracker>();

			tracker->UpdateEntity();
			m_entitySpatialHashTable.Insert(entity, tracker);
		}
		// ------------------------------------------------------------------------------------------------------------------
	}

	void EntityHierarchy::RenderUpdate(const float& interpT)
	{
		for (TObjectPtr<Entity>& entity : m_dynamicEntities)
		{
			RenderDefinition* renderDef = entity->GetComponentPtr<RenderDefinition>();
			if (renderDef != nullptr)
			{
				renderDef->UpdateRenderEntity(interpT);
			}
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
			*m_defaultMaterial->GetReflection().GetFieldValueWithName<std::string>("m_name") = "mat_default";

			m_defaultMaterial->SaveToDataNode(defaultMaterialNode);
		}
		else
		{
			defaultMat->FillFromDataNode(defaultMaterialNode);
			m_defaultMaterial = defaultMat;
		}
	}
}