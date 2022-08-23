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

		struct FramebufferData
		{
			Framebuffer m_framebuffer = 0;
			CLib::Vector<u32> m_viewUniqueIds{};
		};

		PARABLIT_API void CreateFramebuffer(FramebufferData& outFramebufferData, const FramebufferDesc& desc);

		Device* m_device = nullptr;
		std::unordered_map<FramebufferDesc, FramebufferData, FramebufferDescHasher> m_cache;
	};
}