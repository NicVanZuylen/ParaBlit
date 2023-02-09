#include "Entity/EntityHierarchy.h"
#include "Entity/Component/Transform.h"

#include "Engine.ParaBlit/ICommandContext.h"

namespace Eng
{
	EntityHierarchy::EntityHierarchy(CLib::Allocator* allocator, PB::IRenderer* renderer)
	{
		Init(allocator, renderer);
	}

	EntityHierarchy::~EntityHierarchy()
	{

	}

	void EntityHierarchy::Init(CLib::Allocator* allocator, PB::IRenderer* renderer)
	{
		m_allocator = allocator;
		m_renderer = renderer;

		// Initialize bounding volume hierarchies.
		{
			BoundingVolumeHierarchy::CreateDesc bvhDesc{};
			bvhDesc.m_desiredMaxDepth = 50;

			bvhDesc.m_toleranceDistanceX = 0.05f;
			bvhDesc.m_toleranceDistanceY = 0.05f;

			bvhDesc.m_toleranceStepX = 0.05f;
			bvhDesc.m_toleranceStepZ = 0.05f;

			bvhDesc.m_toleranceDistanceY = 0.2f;
			bvhDesc.m_toleranceStepY = 0.2f;

			m_renderHierarchy.Init(renderer, allocator, bvhDesc);
			m_entityHierarchy.Init(allocator, bvhDesc);
		}
	}

	void EntityHierarchy::Destroy()
	{
		for (auto& entity : m_entityAllocations)
		{
			m_entityAllocator.Free(entity);
		}
		m_entityAllocations.Clear();
	}

	Entity* EntityHierarchy::AddEntity(const glm::vec3& position)
	{
		Entity* newEntity = m_entityAllocator.Alloc<Entity>(this);
		Transform* transform = newEntity->AddComponent<Transform>();
		transform->SetPosition(position);

		m_entityAllocations.PushBack(newEntity);
		return newEntity;
	}

	void EntityHierarchy::BakeTrees()
	{
		//m_entityHierarchy.Build();
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
}