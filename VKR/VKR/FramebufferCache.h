#pragma once
#include "ParaBlitDefs.h"
#include "RenderPassCache.h"
#include "ITextureViewCache.h"

#include <unordered_map>

namespace PB
{
	class Device;

	struct FramebufferDesc
	{
		RenderPass m_renderPass = nullptr;
		u32 m_width = 0;
		u32 m_height = 0;
		u64 m_attachmentCount = 0;
		TextureView* m_attachmentViews = nullptr;

		bool operator == (const FramebufferDesc& other) const;
	};

	using Framebuffer = void*;

	struct FramebufferDescHasher
	{
		size_t operator()(const FramebufferDesc& desc) const;
	};

	class FramebufferCache
	{
	public:

		void Init(Device* device);

		void Destroy();

		PARABLIT_API Framebuffer GetFramebuffer(const FramebufferDesc& desc);

	private:

		PARABLIT_API Framebuffer CreateFramebuffer(const FramebufferDesc& desc);

		Device* m_device = nullptr;
		std::unordered_map<FramebufferDesc, Framebuffer, FramebufferDescHasher> m_cache;
	};
}