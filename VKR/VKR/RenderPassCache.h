#pragma once
#include "IRenderPassCache.h"
#include "ParaBlitApi.h"
#include "vulkan/vulkan.h"
#include "Texture.h"

#include <unordered_map>

namespace PB 
{
	struct RenderPassHasher
	{
		// Hash function for the map.
		size_t operator()(const RenderPassDesc& desc) const;
	};

	class Device;

	class RenderPassCache : public IRenderPassCache
	{
	public:

		RenderPassCache();

		~RenderPassCache();

		PARABLIT_API void Init(Device* device);

		PARABLIT_API void Destroy();

		PARABLIT_API RenderPass GetRenderPass(const RenderPassDesc& desc) override;

	private:

		PARABLIT_API inline VkRenderPass CreateRenderPass(const RenderPassDesc& desc);

		std::unordered_map<RenderPassDesc, VkRenderPass, RenderPassHasher> m_cache;
		Device* m_device = nullptr;
	};
}