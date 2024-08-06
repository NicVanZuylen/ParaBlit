#include "Entity/EntityHierarchy.h"
#include "Entity/Component/Transform.h"
#include "Entity/Component/StaticEntityTracker.h"

#include "Engine.ParaBlit/ICommandContext.h"

#include "Resource/AssetStreamer.h"

namespace Eng
{
	EntityHierarchy::EntityHierarchy(CLib::Allocator* allocator, PB::IRenderer* renderer, AssetStreamer* streamer)
	{
		Init(allocator, renderer, streamer);
	}

	EntityHierarchy::~EntityHierarchy()
	{

	}

	void EntityHierarchy::Init(CLib::Allocator* allocator, PB::IRenderer* renderer, AssetStreamer* streamer)
	{
		m_allocator = allocator;
		m_renderer = renderer;
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
	}

	void EntityHierarchy::Destroy()
	{
		for (auto& entity : m_entityAllocations)
		{
			m_entityAllocator.Free(entity);
		}
		m_entityAllocations.clear();

		m_entityBoundingVolumeHierarchy.Destroy();
		m_renderHierarchy.Destroy();
	}

	Entity* EntityHierarchy::AddEntity(const Vector3f& position, const char* name)
	{
		Entity* newEntity = m_entityAllocator.Alloc<Entity>(this, name);
		Transform* transform = newEntity->AddComponent<Transform>();
		transform->SetPosition(position);

		newEntity->AddComponent<StaticEntityTracker>();

		m_entityAllocations.insert(newEntity);
		return newEntity;
	}

	void EntityHierarchy::DestroyEntity(Entity* entity)
	{
		entity->GetComponent<StaticEntityTracker>()->UncommitEntity();

		UpdateTrees();
	}

	void EntityHierarchy::CommitEntity(Entity* entity)
	{
		StaticEntityTracker* tracker = entity->GetComponent<StaticEntityTracker>();
		if (tracker != nullptr)
		{
			tracker->CommitEntity();
		}
	}

	void EntityHierarchy::UpdateTrees()
	{
		m_entityBoundingVolumeHierarchy.UpdateTree();
		m_renderHierarchy.UpdateTree();

		PB::CommandContextDesc contextDesc{};
		contextDesc.m_renderer = m_renderer;
		contextDesc.m_usage = PB::ECommandContextUsage::COMPUTE;
		contextDesc.m_flags = PB::ECommandContextFlags::PRIORITY;

		PB::SCommandContext initCmdContext(m_renderer);
		initCmdContext->Init(contextDesc);
		initCmdContext->Begin();

		m_renderHierarchy.BakeBatches(initCmdContext.GetContext());

		initCmdContext->End();
		initCmdContext->Return();
	}

	void EntityHierarchy::BakeTrees()
	{
		m_entityBoundingVolumeHierarchy.Build();
		m_renderHierarchy.Build();

		PB::CommandContextDesc contextDesc{};
		contextDesc.m_renderer = m_renderer;
		contextDesc.m_usage = PB::ECommandContextUsage::COMPUTE;
		contextDesc.m_flags = PB::ECommandContextFlags::PRIORITY;

		PB::SCommandContext initCmdContext(m_renderer);
		initCmdContext->Init(contextDesc);
		initCmdContext->Begin();

		m_renderHierarchy.BakeBatches(initCmdContext.GetContext());

		initCmdContext->End();
		initCmdContext->Return();
	}

	void EntityHierarchy::UpdateBVHTest(Entity* entityToMove)
	{
		printf_s("Moving Entity: %s\n", entityToMove->GetName());

		entityToMove->GetComponent<Transform>()->Translate(glm::vec3(1.0f, 0.0f, 0.0f));
		entityToMove->GetComponent<StaticEntityTracker>()->UpdateEntity();

		UpdateTrees();
	}
}