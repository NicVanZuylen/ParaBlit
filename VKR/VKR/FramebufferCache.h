#pragma once
#include "IFrameBufferCache.h"
#include "RenderPassCache.h"

#include <unordered_map>

namespace PB
{
	class Device;

	struct FramebufferDescHasher
	{
		size_t operator()(const FramebufferDesc& desc) const;
	};

	class FramebufferCache : public IFramebufferCache
	{
	public:

		void Init(Device* device);

		void Destroy();

		Framebuffer GetFramebuffer(const FramebufferDesc& desc) override;

	private:

		PARABLIT_API Framebuffer CreateFramebuffer(const FramebufferDesc& desc);

		Device* m_device = nullptr;
		std::unordered_map<FramebufferDesc, Framebuffer, FramebufferDescHasher> m_cache;
	};
}