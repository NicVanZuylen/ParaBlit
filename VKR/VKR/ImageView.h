#pragma once
#include "ITextureViewCache.h"
#include "ICommandContext.h"

#include "vulkan/vulkan.h"

#include <unordered_map>

namespace PB
{
	class Device;

	struct TextureViewDescHasher
	{
		size_t operator()(const TextureViewDesc& desc) const;				// Unordered map hashing operator.
	};

	struct ViewData
	{
		VkImageView m_view = VK_NULL_HANDLE;
	};

	class TextureViewCache : public ITextureViewCache
	{
	public:

		TextureViewCache();

		~TextureViewCache();

		void Init(Device* device);

		void Destroy();

		PARABLIT_API TextureView GetView(const TextureViewDesc& desc) override;

	private:

		PARABLIT_API inline ViewData CreateView(const TextureViewDesc& desc);

		Device* m_device;
		std::unordered_map<TextureViewDesc, ViewData, TextureViewDescHasher> m_viewCache;
	};
}

