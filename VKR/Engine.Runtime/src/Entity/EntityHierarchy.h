#pragma once
#include "CLib/FixedBlockAllocator.h"
#include "Entity/EntityBoundingVolumeHierarchy.h"
#include "WorldRender/RenderBoundingVolumeHierarchy.h"

namespace Eng
{
	class AssetStreamer;

	class EntityHierarchy
	{
	public:

		EntityHierarchy() = default;
		EntityHierarchy(CLib::Allocator* allocator, PB::IRenderer* renderer, AssetStreamer* streamer);
		~EntityHierarchy();

		void Init(CLib::Allocator* allocator, PB::IRenderer* renderer, AssetStreamer* streamer);
		void Destroy();

		Entity* AddEntity(const glm::vec3& position);
		inline Entity* AddEntity(const glm::vec4& position)
		{
			return AddEntity(glm::vec3(position.x, position.y, position.z));
		}

		inline RenderBoundingVolumeHierarchy& GetRenderHierarchy() { return m_renderHierarchy; }
		inline EntityBoundingVolumeHierarchy& GetEntityBoundingVolumeHierarchy() { return m_entityBoundingVolumeHierarchy; }

		/*
		Description: Commit the entity at its current position with its current rendering properites to the scene as a static object.
		*/
		void CommitEntity(Entity* entity);

		void BakeTrees();

		void BuildEntityBVH() { m_entityBoundingVolumeHierarchy.Build(); }

	private:

		CLib::Allocator* m_allocator = nullptr;
		PB::IRenderer* m_renderer = nullptr;
		AssetStreamer* m_streamer = nullptr;
		RenderBoundingVolumeHierarchy m_renderHierarchy;
		EntityBoundingVolumeHierarchy m_entityBoundingVolumeHierarchy;
		CLib::FixedBlockAllocator m_entityAllocator{ sizeof(Entity), sizeof(Entity) * 512 };
		CLib::Vector<Entity*, 64, 64> m_entityAllocations;

	};
}